// Package transport implements CapabilitiesService over capabilities/api.
package transport

import (
	"context"

	"connectrpc.com/connect"

	capabilitiesv1 "github.com/aedst/server/gen/go/aedst/capabilities/v1"
	"github.com/aedst/server/internal/modules/capabilities/api"
	"github.com/aedst/server/internal/platform/httpserver"
)

type Handler struct {
	service api.Service
}

func NewHandler(service api.Service) *Handler { return &Handler{service: service} }

// GetCapabilities is public: a client learns it must update before it can sign in.
func (h *Handler) GetCapabilities(ctx context.Context, req *connect.Request[capabilitiesv1.GetCapabilitiesRequest]) (*connect.Response[capabilitiesv1.GetCapabilitiesResponse], error) {
	platform, version, _ := httpserver.ParseClientHeader(req.Header().Get(httpserver.ClientHeader))
	caps, err := h.service.Get(ctx, platform, version)
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	out := &capabilitiesv1.GetCapabilitiesResponse{
		ServerVersion: caps.ServerVersion,
		Clients:       make(map[string]*capabilitiesv1.ClientVersions, len(caps.Clients)),
		Features:      make(map[string]*capabilitiesv1.Feature, len(caps.Features)),
		UpdateStatus:  statusToProto(caps.Status),
	}
	for p, v := range caps.Clients {
		out.Clients[p] = &capabilitiesv1.ClientVersions{Minimum: v.Minimum, Recommended: v.Recommended}
	}
	for name, f := range caps.Features {
		out.Features[name] = &capabilitiesv1.Feature{Enabled: f.Enabled, Params: f.Params, RequiredClientVersion: f.RequiredClientVersion}
	}
	return connect.NewResponse(out), nil
}

func statusToProto(s api.UpdateStatus) capabilitiesv1.UpdateStatus {
	switch s {
	case api.StatusCurrent:
		return capabilitiesv1.UpdateStatus_UPDATE_STATUS_CURRENT
	case api.StatusRecommended:
		return capabilitiesv1.UpdateStatus_UPDATE_STATUS_RECOMMENDED
	default:
		return capabilitiesv1.UpdateStatus_UPDATE_STATUS_UNSPECIFIED
	}
}
