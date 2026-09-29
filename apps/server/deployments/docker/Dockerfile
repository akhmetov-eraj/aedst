# Multi-stage build for the aedst-server binary. Build context is
# apps/server (see Taskfile.yml/server-ci.yml's docker build invocations,
# which pass `-f deployments/docker/Dockerfile .` from apps/server).
FROM golang:1.27-alpine AS build
WORKDIR /src

RUN apk add --no-cache git

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -trimpath -ldflags="-s -w" -o /out/aedst-server ./cmd/server

# Distroless: no shell, no package manager — smaller attack surface than
# alpine for the runtime image, matching how the binary is deployed (no
# in-container debugging; use `kubectl debug` / a sidecar instead).
FROM gcr.io/distroless/static-debian12:nonroot AS runtime
COPY --from=build /out/aedst-server /aedst-server

EXPOSE 8080
ENTRYPOINT ["/aedst-server"]
