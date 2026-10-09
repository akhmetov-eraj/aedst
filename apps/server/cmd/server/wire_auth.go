package main

import (
	"context"
	"time"

	platformauth "github.com/aedst/server/internal/platform/auth"
)

// buildVerifier wires the authentication back-ends: first-party tokens and/or
// an external OIDC provider, verified through one platformauth.Verifier. The
// first-party AuthService itself is the auth module's (modules/auth/module).
func (a *app) buildVerifier(ctx context.Context) error {
	cfg := a.cfg
	if cfg.Auth.FirstPartyEnabled {
		logFirstPartySunset(ctx, a.logger, cfg)
		firstParty, err := platformauth.NewTokenService(platformauth.Config{
			Issuer:          cfg.Obs.ServiceName,
			SigningKey:      cfg.Auth.JWTSigningKey,
			PreviousKey:     cfg.Auth.JWTPreviousSigningKey,
			AccessTokenTTL:  time.Duration(cfg.Auth.AccessTokenTTLMinutes) * time.Minute,
			RefreshTokenTTL: time.Duration(cfg.Auth.RefreshTokenTTLHours) * time.Hour,
		})
		if err != nil {
			return err
		}
		a.env.FirstParty = firstParty
	}
	var oidcVerifier *platformauth.OIDCVerifier
	if cfg.OIDC.Issuer != "" {
		var err error
		oidcVerifier, err = platformauth.NewOIDCVerifier(platformauth.OIDCConfig{
			Issuer:     cfg.OIDC.Issuer,
			Audience:   cfg.OIDC.Audience,
			JWKSURL:    cfg.OIDC.JWKSURL,
			Algorithms: cfg.OIDC.Algorithms,
			TokenTypes: cfg.OIDC.TokenTypes,
			RolesClaim: cfg.OIDC.RolesClaim,
			RolesOrg:   cfg.OIDC.RolesOrg,
			ClockSkew:  time.Duration(cfg.OIDC.ClockSkewSeconds) * time.Second,
		})
		if err != nil {
			return err
		}
		a.onClose(func() { _ = oidcVerifier.Close(context.WithoutCancel(ctx)) })
	}
	verifier, err := platformauth.NewMultiVerifier(a.env.FirstParty, oidcVerifier)
	if err != nil {
		return err
	}
	a.verifier = verifier
	return nil
}
