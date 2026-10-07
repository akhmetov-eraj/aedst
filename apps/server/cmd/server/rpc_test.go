package main

import (
	"bytes"
	"context"
	"go/ast"
	"go/parser"
	"go/token"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"

	"connectrpc.com/connect"
	"github.com/go-chi/chi/v5"

	tasksv1 "github.com/aedst/server/gen/go/aedst/tasks/v1"
	tasksv1connect "github.com/aedst/server/gen/go/aedst/tasks/v1/tasksv1connect"
)

type stubTasks struct {
	tasksv1connect.UnimplementedTasksServiceHandler
	called bool
}

func (s *stubTasks) GetTask(context.Context, *connect.Request[tasksv1.GetTaskRequest]) (*connect.Response[tasksv1.GetTaskResponse], error) {
	s.called = true
	return connect.NewResponse(&tasksv1.GetTaskResponse{}), nil
}

// The mounter must put protovalidate in front of every handler: a request
// violating the proto constraints never reaches the implementation.
func TestMountService_AppliesProtovalidate(t *testing.T) {
	router := chi.NewRouter()
	m := newRPCMounter(router, nil, nil, 1<<20, true)
	stub := &stubTasks{}
	mountService(m, tasksv1connect.NewTasksServiceHandler, tasksv1connect.TasksServiceHandler(stub), 0)
	srv := httptest.NewServer(router)
	defer srv.Close()

	client := tasksv1connect.NewTasksServiceClient(srv.Client(), srv.URL)
	_, err := client.GetTask(context.Background(), connect.NewRequest(&tasksv1.GetTaskRequest{Id: "not-a-uuid"}))
	if connect.CodeOf(err) != connect.CodeInvalidArgument || stub.called {
		t.Fatalf("err = %v, called = %v; want InvalidArgument before the handler", err, stub.called)
	}
	if _, err := client.GetTask(context.Background(), connect.NewRequest(&tasksv1.GetTaskRequest{Id: "0190a3d8-7a5e-7c3b-8f2a-3c4d5e6f7a8b"})); err != nil || !stub.called {
		t.Fatalf("a valid request failed: %v", err)
	}
	if got := m.services(); len(got) != 1 || got[0] != tasksv1connect.TasksServiceName {
		t.Errorf("services() = %v", got)
	}
}

// Connect-Protocol-Version is required (CSRF defense in depth): a bare JSON
// POST — what an HTML form or a simple cross-site fetch could send — fails.
func TestMountService_RequiresConnectProtocolHeader(t *testing.T) {
	router := chi.NewRouter()
	m := newRPCMounter(router, nil, nil, 1<<20, true)
	mountService(m, tasksv1connect.NewTasksServiceHandler, tasksv1connect.TasksServiceHandler(&stubTasks{}), 0)

	req := httptest.NewRequest(http.MethodPost, tasksv1connect.TasksServiceGetTaskProcedure, bytes.NewReader([]byte(`{"id":"0190a3d8-7a5e-7c3b-8f2a-3c4d5e6f7a8b"}`)))
	req.Header.Set("Content-Type", "application/json")
	rec := httptest.NewRecorder()
	router.ServeHTTP(rec, req)
	if rec.Code == http.StatusOK {
		t.Fatalf("status = %d without Connect-Protocol-Version, want a rejection", rec.Code)
	}
}

func TestMountService_PerServiceReadLimit(t *testing.T) {
	router := chi.NewRouter()
	m := newRPCMounter(router, nil, nil, 1<<20, false)
	mountService(m, tasksv1connect.NewTasksServiceHandler, tasksv1connect.TasksServiceHandler(&stubTasks{}), 64)
	srv := httptest.NewServer(router)
	defer srv.Close()
	client := tasksv1connect.NewTasksServiceClient(srv.Client(), srv.URL)
	_, err := client.GetTask(context.Background(), connect.NewRequest(&tasksv1.GetTaskRequest{Id: strings.Repeat("x", 200)}))
	if connect.CodeOf(err) != connect.CodeResourceExhausted {
		t.Fatalf("err = %v, want ResourceExhausted from the per-service limit", err)
	}
}

var generatedHandlerCtor = regexp.MustCompile(`^New\w+ServiceHandler$`)

// Every generated Connect handler constructor in cmd/server must be passed to
// mountService (never called directly, which would bypass the shared chain
// and its protovalidate interceptor), and router.Mount may only receive the
// non-RPC handlers (gRPC health, reflection).
func TestEveryConnectHandlerIsMountedThroughTheMounter(t *testing.T) {
	files, err := filepath.Glob("*.go")
	if err != nil {
		t.Fatal(err)
	}
	fset := token.NewFileSet()
	mounted := 0
	for _, name := range files {
		if strings.HasSuffix(name, "_test.go") {
			continue
		}
		src, err := os.ReadFile(name)
		if err != nil {
			t.Fatal(err)
		}
		file, err := parser.ParseFile(fset, name, src, 0)
		if err != nil {
			t.Fatal(err)
		}
		ast.Inspect(file, func(n ast.Node) bool {
			call, ok := n.(*ast.CallExpr)
			if !ok {
				return true
			}
			switch fn := call.Fun.(type) {
			case *ast.SelectorExpr:
				if generatedHandlerCtor.MatchString(fn.Sel.Name) {
					t.Errorf("%s: %s called directly — mount Connect services with mountService so protovalidate always applies", fset.Position(call.Pos()), fn.Sel.Name)
				}
				if fn.Sel.Name == "Mount" {
					for _, arg := range call.Args {
						if inner, ok := arg.(*ast.CallExpr); ok {
							if sel, ok := inner.Fun.(*ast.SelectorExpr); ok {
								if pkg, ok := sel.X.(*ast.Ident); ok && pkg.Name != "grpchealth" && pkg.Name != "grpcreflect" {
									t.Errorf("%s: router.Mount(%s.%s(...)) — RPC services go through mountService", fset.Position(call.Pos()), pkg.Name, sel.Sel.Name)
								}
							}
						}
					}
				}
			case *ast.Ident:
				if fn.Name == "mountService" && name == "main.go" {
					mounted++
				}
			}
			return true
		})
	}
	if mounted < 5 {
		t.Errorf("main.go mounts %d services through mountService, expected every API service", mounted)
	}
}

// The interceptor chain (auth, roles, rate limits, idempotency) is built from
// connect.UnaryInterceptorFunc, which does NOT wrap streaming handlers: a
// streaming RPC would skip authentication. Until the chain implements
// WrapStreamingHandler, no mounted service may declare a streaming method.
func TestNoStreamingProceduresWhileInterceptorsAreUnaryOnly(t *testing.T) {
	router := chi.NewRouter()
	m := newRPCMounter(router, nil, nil, 1<<20, true)
	mountService(m, tasksv1connect.NewTasksServiceHandler, tasksv1connect.TasksServiceHandler(&stubTasks{}), 0)
	m.mounted = append(m.mounted,
		"aedst.auth.v1.AuthService", "aedst.notifications.v1.NotificationsService",
		"aedst.search.v1.SearchService", "aedst.media.v1.MediaService", "aedst.admin.v1.AdminService")
	if s := m.streamingProcedures(); len(s) != 0 {
		t.Fatalf("streaming procedures %v: make every interceptor stream-aware first", s)
	}
}
