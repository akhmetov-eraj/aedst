package httpserver

import (
	"context"
	"fmt"
	"log/slog"
	"strings"

	"connectrpc.com/connect"
	"golang.org/x/mod/semver"
	"google.golang.org/genproto/googleapis/rpc/errdetails"
)

// ClientHeader is the request header a first-party client sends to identify
// itself: "<platform>/<semver>", e.g. "android/1.4.2". Platform is one of
// config.ClientPlatforms.
const ClientHeader = "X-Aedst-Client"

// ClientUpgradeRequiredReason is the google.rpc.ErrorInfo reason (domain
// "aedst") attached to the failed_precondition error returned to a client
// older than the configured minimum. Clients match on it to show a forced
// upgrade screen instead of a generic error.
const ClientUpgradeRequiredReason = "CLIENT_UPGRADE_REQUIRED"

// ClientVersionInterceptor rejects RPCs from a self-identifying client whose
// version is below the minimum configured for its platform (see ADR-0009). It
// is the server half of the API compatibility policy: proto rules keep old
// clients working across additive changes, and this gate is how a release
// that must break them (or drop support for an ancient build) says so
// explicitly, instead of the client failing in some confusing way.
//
// A request that omits the header, names an unknown platform, or carries an
// unparsable version is allowed through: curl, health probes and pre-header
// builds must keep working, and this is a support-window control, not a
// security boundary. minVersions maps platform -> semver without the "v"
// prefix; nil/empty disables the gate.
func ClientVersionInterceptor(minVersions map[string]string, logger *slog.Logger) connect.UnaryInterceptorFunc {
	mins := make(map[string]string, len(minVersions))
	for platform, version := range minVersions {
		mins[strings.ToLower(platform)] = "v" + strings.TrimPrefix(version, "v")
	}
	return func(next connect.UnaryFunc) connect.UnaryFunc {
		if len(mins) == 0 {
			return next
		}
		return func(ctx context.Context, req connect.AnyRequest) (connect.AnyResponse, error) {
			platform, version, ok := parseClientHeader(req.Header().Get(ClientHeader))
			if !ok {
				return next(ctx, req)
			}
			minimum, gated := mins[platform]
			if !gated || semver.Compare(version, minimum) >= 0 {
				return next(ctx, req)
			}
			logger.InfoContext(ctx, "client version rejected",
				slog.String("platform", platform), slog.String("version", version), slog.String("minimum", minimum))
			return nil, clientUpgradeRequired(platform, version, minimum)
		}
	}
}

// parseClientHeader splits "platform/semver" and returns the semver with a
// leading "v" (the form golang.org/x/mod/semver requires).
func parseClientHeader(header string) (platform, version string, ok bool) {
	platform, version, found := strings.Cut(strings.TrimSpace(header), "/")
	if !found {
		return "", "", false
	}
	version = "v" + strings.TrimPrefix(strings.TrimSpace(version), "v")
	if !semver.IsValid(version) {
		return "", "", false
	}
	return strings.ToLower(strings.TrimSpace(platform)), version, true
}

func clientUpgradeRequired(platform, version, minimum string) *connect.Error {
	err := connect.NewError(connect.CodeFailedPrecondition,
		fmt.Errorf("this version of the %s app is no longer supported; update to %s or newer", platform, strings.TrimPrefix(minimum, "v")))
	if detail, derr := connect.NewErrorDetail(&errdetails.ErrorInfo{
		Reason: ClientUpgradeRequiredReason,
		Domain: "aedst",
		Metadata: map[string]string{
			"platform":        platform,
			"client_version":  strings.TrimPrefix(version, "v"),
			"minimum_version": strings.TrimPrefix(minimum, "v"),
		},
	}); derr == nil {
		err.AddDetail(detail)
	}
	return err
}
