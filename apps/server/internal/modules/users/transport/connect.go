// Package transport implements the users module's ConnectRPC handlers. It
// depends only on modules/users/api (see docs/modules.md).
package transport

import (
	"context"

	"connectrpc.com/connect"
	"google.golang.org/protobuf/types/known/timestamppb"

	authv1 "github.com/aedst/server/gen/go/aedst/auth/v1"
	usersv1 "github.com/aedst/server/gen/go/aedst/users/v1"
	"github.com/aedst/server/internal/modules/users/api"
	"github.com/aedst/server/internal/platform/httpserver"
	"github.com/aedst/server/internal/shared/ids"
)

type Handler struct {
	service api.Service
}

func NewHandler(service api.Service) *Handler {
	return &Handler{service: service}
}

func (h *Handler) RegisterUser(ctx context.Context, req *connect.Request[usersv1.RegisterUserRequest]) (*connect.Response[usersv1.RegisterUserResponse], error) {
	user, tokens, err := h.service.RegisterUser(ctx, req.Msg.GetEmail(), req.Msg.GetPassword(), req.Msg.GetDeviceId())
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&usersv1.RegisterUserResponse{
		User:   toProtoUser(user),
		Tokens: toProtoTokenPair(tokens),
	}), nil
}

func (h *Handler) GetUser(ctx context.Context, req *connect.Request[usersv1.GetUserRequest]) (*connect.Response[usersv1.GetUserResponse], error) {
	userID, err := ids.Parse(req.Msg.GetUserId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	user, err := h.service.GetUser(ctx, userID)
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&usersv1.GetUserResponse{User: toProtoUser(user)}), nil
}

func (h *Handler) GetUserByEmail(ctx context.Context, req *connect.Request[usersv1.GetUserByEmailRequest]) (*connect.Response[usersv1.GetUserByEmailResponse], error) {
	user, err := h.service.GetUserByEmail(ctx, req.Msg.GetEmail())
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&usersv1.GetUserByEmailResponse{User: toProtoUser(user)}), nil
}

func toProtoUser(user api.User) *usersv1.User {
	return &usersv1.User{
		Id:        user.ID.String(),
		Email:     user.Email,
		Roles:     user.Roles,
		CreatedAt: timestamppb.New(user.CreatedAt),
	}
}

func toProtoTokenPair(pair api.TokenPair) *authv1.TokenPair {
	return &authv1.TokenPair{
		AccessToken:                  pair.AccessToken,
		RefreshToken:                 pair.RefreshToken,
		AccessTokenExpiresInSeconds:  int64(pair.AccessTTL.Seconds()),
		RefreshTokenExpiresInSeconds: int64(pair.RefreshTTL.Seconds()),
	}
}
