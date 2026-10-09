package modulekit

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"runtime/debug"
	"strings"

	"connectrpc.com/connect"
	"connectrpc.com/validate"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
)

// Mounter is the ONLY way a ConnectRPC service is mounted (rule: proto
// contract -> protovalidate -> Connect interceptor). Every handler it mounts
// gets the same interceptor chain with connectrpc.com/validate built in, the
// per-service message limit and the Connect protocol-header requirement, so a
// service cannot be mounted without request validation by construction.
// architecturetest's TestEveryConnectHandlerIsMountedThroughTheMounter fails
// the build if a generated New*ServiceHandler is called outside Mount.
type Mounter struct {
	router         *http.ServeMux
	before, after  []connect.Interceptor
	defaultMaxRead int
	requireHeader  bool
	mounted        []string
	logger         *slog.Logger
}

// NewMounter builds a mounter whose chain is before + protovalidate + after.
func NewMounter(router *http.ServeMux, before, after []connect.Interceptor, defaultMaxRead int, requireHeader bool) *Mounter {
	return &Mounter{router: router, before: before, after: after, defaultMaxRead: defaultMaxRead, requireHeader: requireHeader, logger: slog.Default()}
}

// WithLogger sets the logger a recovered handler panic is reported to.
func (m *Mounter) WithLogger(l *slog.Logger) *Mounter {
	m.logger = l
	return m
}

func (m *Mounter) options(maxRead int) []connect.HandlerOption {
	chain := make([]connect.Interceptor, 0, len(m.before)+len(m.after)+1)
	chain = append(chain, m.before...)
	chain = append(chain, validate.NewInterceptor())
	chain = append(chain, m.after...)
	if maxRead <= 0 {
		maxRead = m.defaultMaxRead
	}
	opts := []connect.HandlerOption{
		connect.WithInterceptors(chain...),
		connect.WithReadMaxBytes(maxRead),
		connect.WithRecover(m.recoverPanic),
	}
	if m.requireHeader {
		opts = append(opts, connect.WithRequireConnectProtocolHeader())
	}
	return opts
}

// Mount mounts a generated Connect service. maxRead caps one request message
// for this service (0 = the server default, AEDST_MAX_REQUEST_BYTES).
func Mount[H any](m *Mounter, newHandler func(H, ...connect.HandlerOption) (string, http.Handler), impl H, maxRead int) {
	path, handler := newHandler(impl, m.options(maxRead)...)
	m.router.Handle(path, handler)
	m.mounted = append(m.mounted, strings.Trim(path, "/"))
}

// Services lists the mounted service names (for gRPC health/reflection).
func (m *Mounter) Services() []string { return append([]string(nil), m.mounted...) }

// StreamingProcedures returns the mounted procedures that stream, from the
// protobuf registry (they are exempt from the unary request/write timeouts).
func (m *Mounter) StreamingProcedures() map[string]bool {
	out := map[string]bool{}
	for _, name := range m.mounted {
		desc, err := protoregistry.GlobalFiles.FindDescriptorByName(protoreflect.FullName(name))
		if err != nil {
			continue
		}
		sd, ok := desc.(protoreflect.ServiceDescriptor)
		if !ok {
			continue
		}
		methods := sd.Methods()
		for i := range methods.Len() {
			if md := methods.Get(i); md.IsStreamingClient() || md.IsStreamingServer() {
				out["/"+name+"/"+string(md.Name())] = true
			}
		}
	}
	return out
}

// VerifyKnown fails startup when a mounted service's descriptor is missing from
// known: the role policy could not name its procedures (a module forgot Files).
func (m *Mounter) VerifyKnown(known map[string]bool) error {
	for _, name := range m.mounted {
		found := false
		for p := range known {
			if strings.HasPrefix(p, "/"+name+"/") {
				found = true
				break
			}
		}
		if !found {
			return fmt.Errorf("service %s is mounted but its descriptor is not in any module's Files", name)
		}
	}
	return nil
}

// recoverPanic turns a panic in a handler into CodeInternal for that one call:
// without it net/http recovers, logs to stderr and aborts the connection, which a
// client sees as a transport failure and the structured log and trace never see.
// The panic value and the stack are logged with the request context (so with its
// trace_id); the client gets the same generic message as any internal error.
func (m *Mounter) recoverPanic(ctx context.Context, spec connect.Spec, _ http.Header, p any) error {
	m.logger.ErrorContext(ctx, "rpc panic recovered",
		slog.String("procedure", spec.Procedure),
		slog.Any("panic", p),
		slog.String("stack", string(debug.Stack())))
	return connect.NewError(connect.CodeInternal, errors.New("internal error"))
}
