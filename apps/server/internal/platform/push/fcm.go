package push

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"

	"golang.org/x/oauth2"
	"golang.org/x/oauth2/jwt"
)

const (
	fcmScope       = "https://www.googleapis.com/auth/firebase.messaging"
	fcmDefaultBase = "https://fcm.googleapis.com"
	googleTokenURL = "https://oauth2.googleapis.com/token"
)

// FCMConfig configures Firebase Cloud Messaging HTTP v1
// (AEDST_PUSH_FCM_*). The service account needs the "Firebase Cloud
// Messaging API Admin" role (cloudmessaging.messages.create).
type FCMConfig struct {
	// CredentialsJSON is the service-account key JSON; CredentialsFile is a
	// path to it. One of them is required.
	CredentialsJSON string
	CredentialsFile string
	// ProjectID overrides the service account's project_id.
	ProjectID string
	// BaseURL is for tests; empty uses https://fcm.googleapis.com.
	BaseURL string
}

// FCM sends through the FCM HTTP v1 API with a service-account OAuth token
// (two-legged JWT bearer grant, RFC 7523, via golang.org/x/oauth2/jwt; the
// token is cached and refreshed by oauth2.ReuseTokenSource).
type FCM struct {
	projectID string
	baseURL   string
	tokens    oauth2.TokenSource
	client    *http.Client
}

type serviceAccount struct {
	Type         string `json:"type"`
	ProjectID    string `json:"project_id"`
	PrivateKeyID string `json:"private_key_id"`
	PrivateKey   string `json:"private_key"`
	ClientEmail  string `json:"client_email"`
	TokenURI     string `json:"token_uri"`
}

func NewFCM(ctx context.Context, cfg FCMConfig, client *http.Client) (*FCM, error) {
	raw := []byte(cfg.CredentialsJSON)
	if len(raw) == 0 && cfg.CredentialsFile != "" {
		var err error
		if raw, err = os.ReadFile(cfg.CredentialsFile); err != nil {
			return nil, fmt.Errorf("push: fcm credentials: %w", err)
		}
	}
	var sa serviceAccount
	if err := json.Unmarshal(raw, &sa); err != nil {
		return nil, fmt.Errorf("push: fcm credentials: %w", err)
	}
	if sa.Type != "service_account" || sa.ClientEmail == "" || sa.PrivateKey == "" {
		return nil, errors.New("push: fcm credentials must be a service-account key JSON")
	}
	projectID := cfg.ProjectID
	if projectID == "" {
		projectID = sa.ProjectID
	}
	if projectID == "" {
		return nil, errors.New("push: fcm project id is unknown (set AEDST_PUSH_FCM_PROJECT_ID)")
	}
	tokenURL := sa.TokenURI
	if tokenURL == "" {
		tokenURL = googleTokenURL
	}
	jwtCfg := &jwt.Config{
		Email:        sa.ClientEmail,
		PrivateKey:   []byte(sa.PrivateKey),
		PrivateKeyID: sa.PrivateKeyID,
		Scopes:       []string{fcmScope},
		TokenURL:     tokenURL,
	}
	base := cfg.BaseURL
	if base == "" {
		base = fcmDefaultBase
	}
	tokenCtx := context.WithValue(context.WithoutCancel(ctx), oauth2.HTTPClient, client)
	return &FCM{
		projectID: projectID,
		baseURL:   strings.TrimRight(base, "/"),
		tokens:    oauth2.ReuseTokenSource(nil, jwtCfg.TokenSource(tokenCtx)),
		client:    client,
	}, nil
}

type fcmRequest struct {
	Message fcmMessage `json:"message"`
}

type fcmMessage struct {
	Token        string            `json:"token"`
	Notification *fcmNotification  `json:"notification,omitempty"`
	Data         map[string]string `json:"data,omitempty"`
	Android      *fcmAndroid       `json:"android,omitempty"`
}

type fcmNotification struct {
	Title string `json:"title,omitempty"`
	Body  string `json:"body,omitempty"`
}

type fcmAndroid struct {
	Priority    string `json:"priority,omitempty"`
	TTL         string `json:"ttl,omitempty"`
	CollapseKey string `json:"collapse_key,omitempty"`
}

type fcmError struct {
	Error struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
		Status  string `json:"status"`
		Details []struct {
			ErrorCode string `json:"errorCode"`
		} `json:"details"`
	} `json:"error"`
}

func (f *FCM) Send(ctx context.Context, target Target, msg Message) error {
	if target.Token == "" {
		return fmt.Errorf("%w: empty fcm token", ErrInvalidToken)
	}
	data := map[string]string{}
	for k, v := range msg.Data {
		data[k] = v
	}
	data["kind"] = msg.Kind
	data["title"] = msg.Title
	data["body"] = msg.Body
	data["link"] = msg.Link
	data["notification_id"] = msg.NotificationID
	body := fcmRequest{Message: fcmMessage{
		Token:        target.Token,
		Notification: &fcmNotification{Title: msg.Title, Body: msg.Body},
		Data:         data,
		Android:      &fcmAndroid{Priority: "HIGH", CollapseKey: msg.CollapseID},
	}}
	if msg.TTL > 0 {
		body.Message.Android.TTL = strconv.Itoa(int(msg.TTL.Seconds())) + "s"
	}
	payload, err := json.Marshal(body)
	if err != nil {
		return fmt.Errorf("%w: encode fcm message: %w", ErrPermanent, err)
	}
	token, err := f.tokens.Token()
	if err != nil {
		return fmt.Errorf("push: fcm oauth token: %w", err)
	}
	url := f.baseURL + "/v1/projects/" + f.projectID + "/messages:send"
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(payload))
	if err != nil {
		return fmt.Errorf("push: fcm request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	token.SetAuthHeader(req)

	resp, err := f.client.Do(req)
	if err != nil {
		return fmt.Errorf("push: fcm: %w", err)
	}
	defer func() { _ = resp.Body.Close() }()
	if resp.StatusCode == http.StatusOK {
		return nil
	}
	raw, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
	var fe fcmError
	_ = json.Unmarshal(raw, &fe)
	for _, d := range fe.Error.Details {
		if d.ErrorCode == "UNREGISTERED" {
			return fmt.Errorf("%w: fcm UNREGISTERED", ErrInvalidToken)
		}
	}
	if resp.StatusCode == http.StatusBadRequest && strings.Contains(strings.ToLower(fe.Error.Message), "registration token") {
		return fmt.Errorf("%w: fcm %s", ErrInvalidToken, fe.Error.Message)
	}
	return classify("fcm", resp.StatusCode, fe.Error.Status+" "+fe.Error.Message)
}
