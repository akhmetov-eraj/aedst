// Package transport implements the auth module's ConnectRPC handlers. It
// depends only on modules/auth/api — never on application/domain/
// infrastructure directly (see docs/modules.md).
package transport

import (
	"context"

	"connectrpc.com/connect"

	authv1 "github.com/aedst/server/gen/go/aedst/auth/v1"
	"github.com/aedst/server/internal/modules/auth/api"
	"github.com/aedst/server/internal/platform/httpserver"
)

type Handler struct {
	service api.Service
}

func NewHandler(service api.Service) *Handler {
	return &Handler{service: service}
}

func (h *Handler) Login(ctx context.Context, req *connect.Request[authv1.LoginRequest]) (*connect.Response[authv1.LoginResponse], error) {
	userID, pair, err := h.service.Login(ctx, req.Msg.GetEmail(), req.Msg.GetPassword(), req.Msg.GetDeviceId())
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&authv1.LoginResponse{
		UserId: userID.String(),
		Tokens: toProtoTokenPair(pair),
	}), nil
}

func (h *Handler) RefreshToken(ctx context.Context, req *connect.Request[authv1.RefreshTokenRequest]) (*connect.Response[authv1.RefreshTokenResponse], error) {
	pair, err := h.service.RefreshToken(ctx, req.Msg.GetRefreshToken())
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&authv1.RefreshTokenResponse{Tokens: toProtoTokenPair(pair)}), nil
}

func (h *Handler) Logout(ctx context.Context, req *connect.Request[authv1.LogoutRequest]) (*connect.Response[authv1.LogoutResponse], error) {
	if err := h.service.Logout(ctx, req.Msg.GetRefreshToken()); err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&authv1.LogoutResponse{}), nil
}

func toProtoTokenPair(pair api.TokenPair) *authv1.TokenPair {
	return &authv1.TokenPair{
		AccessToken:                  pair.AccessToken,
		RefreshToken:                 pair.RefreshToken,
		AccessTokenExpiresInSeconds:  int64(pair.AccessTTL.Seconds()),
		RefreshTokenExpiresInSeconds: int64(pair.RefreshTTL.Seconds()),
	}
}
