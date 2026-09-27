// Package transport implements the profile module's ConnectRPC handlers. It
// depends only on modules/profile/api (see docs/modules.md).
package transport

import (
	"context"

	"connectrpc.com/connect"
	"google.golang.org/protobuf/types/known/timestamppb"

	profilev1 "github.com/aedst/server/gen/go/aedst/profile/v1"
	"github.com/aedst/server/internal/modules/profile/api"
	platformauth "github.com/aedst/server/internal/platform/auth"
	"github.com/aedst/server/internal/platform/httpserver"
	sharederrors "github.com/aedst/server/internal/shared/errors"
	"github.com/aedst/server/internal/shared/ids"
)

type Handler struct {
	service api.Service
}

func NewHandler(service api.Service) *Handler {
	return &Handler{service: service}
}

func (h *Handler) GetProfile(ctx context.Context, req *connect.Request[profilev1.GetProfileRequest]) (*connect.Response[profilev1.GetProfileResponse], error) {
	userID, err := ids.Parse(req.Msg.GetUserId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	profile, err := h.service.GetProfile(ctx, userID)
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&profilev1.GetProfileResponse{Profile: toProtoProfile(profile)}), nil
}

func (h *Handler) UpdateProfile(ctx context.Context, req *connect.Request[profilev1.UpdateProfileRequest]) (*connect.Response[profilev1.UpdateProfileResponse], error) {
	userID, err := ids.Parse(req.Msg.GetUserId())
	if err != nil {
		return nil, connect.NewError(connect.CodeInvalidArgument, err)
	}

	// Resource-level ownership check: a user may only update their own
	// profile — never trust the client-supplied user_id alone (see project
	// brief section 18 "Authorization").
	identity, ok := platformauth.FromContext(ctx)
	if !ok || identity.UserID != userID {
		return nil, httpserver.ToConnectError(sharederrors.Forbidden("profile.UpdateProfile", "cannot update another user's profile"))
	}

	profile, err := h.service.UpdateProfile(ctx, userID, req.Msg.GetDisplayName(), req.Msg.GetBio(), req.Msg.GetAvatarUrl())
	if err != nil {
		return nil, httpserver.ToConnectError(err)
	}
	return connect.NewResponse(&profilev1.UpdateProfileResponse{Profile: toProtoProfile(profile)}), nil
}

func toProtoProfile(profile api.Profile) *profilev1.Profile {
	return &profilev1.Profile{
		UserId:      profile.UserID.String(),
		DisplayName: profile.DisplayName,
		Bio:         profile.Bio,
		AvatarUrl:   profile.AvatarURL,
		UpdatedAt:   timestamppb.New(profile.UpdatedAt),
	}
}
