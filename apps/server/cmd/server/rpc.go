package main

import (
	"fmt"
	"net/http"
	"strings"

	"connectrpc.com/connect"
	"connectrpc.com/validate"
	"github.com/go-chi/chi/v5"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
)

// rpcMounter is the ONLY way a ConnectRPC service is mounted (rule: proto
// contract -> protovalidate -> Connect interceptor). Every handler it mounts
// gets the same interceptor chain with connectrpc.com/validate built in, the
// per-service message limit and the Connect protocol-header requirement — a
// service cannot be mounted without request validation by construction, and
// TestEveryConnectHandlerIsMountedThroughTheMounter fails the build if
// cmd/server calls a generated New*ServiceHandler outside mountService.
type rpcMounter struct {
	router         chi.Router
	before, after  []connect.Interceptor
	defaultMaxRead int
	requireHeader  bool
	mounted        []string
}

// newRPCMounter builds a mounter whose chain is before + protovalidate + after.
func newRPCMounter(router chi.Router, before, after []connect.Interceptor, defaultMaxRead int, requireHeader bool) *rpcMounter {
	return &rpcMounter{router: router, before: before, after: after, defaultMaxRead: defaultMaxRead, requireHeader: requireHeader}
}

func (m *rpcMounter) options(maxRead int) []connect.HandlerOption {
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
	}
	if m.requireHeader {
		opts = append(opts, connect.WithRequireConnectProtocolHeader())
	}
	return opts
}

// mountService mounts a generated Connect service. maxRead caps one request
// message for this service (0 = the server default, AEDST_MAX_REQUEST_BYTES).
func mountService[H any](m *rpcMounter, newHandler func(H, ...connect.HandlerOption) (string, http.Handler), impl H, maxRead int) {
	path, handler := newHandler(impl, m.options(maxRead)...)
	m.router.Mount(path, handler)
	m.mounted = append(m.mounted, strings.Trim(path, "/"))
}

// services lists the mounted service names (for gRPC health/reflection).
func (m *rpcMounter) services() []string { return append([]string(nil), m.mounted...) }

// streamingProcedures returns the mounted procedures that stream, from the
// protobuf registry (they are exempt from the unary request/write timeouts).
func (m *rpcMounter) streamingProcedures() map[string]bool {
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

// verifyKnown fails startup when a mounted service's descriptor is missing
// from knownProcedures: the role policy could not name its procedures.
func (m *rpcMounter) verifyKnown(known map[string]bool) error {
	for _, name := range m.mounted {
		found := false
		for p := range known {
			if strings.HasPrefix(p, "/"+name+"/") {
				found = true
				break
			}
		}
		if !found {
			return fmt.Errorf("service %s is mounted but its descriptor is not in knownProcedures (cmd/server)", name)
		}
	}
	return nil
}
