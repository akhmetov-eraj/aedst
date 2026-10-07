# Taskfile.yml is now the canonical command runner — CI calls `task <name>`
# directly (see .github/workflows/server-ci.yml), and it adds gates this
# Makefile doesn't have (gen:check, arch, lint:migrations, check/check:full).
# This Makefile is kept only so `make dev`/`make lint`/etc. muscle memory
# keeps working; it is not updated with new gates going forward.
.PHONY: dev down generate lint breaking test integration-test security build clean migrate-up migrate-down

COMPOSE := docker compose -f deployments/docker/docker-compose.yml
DB_URL ?= postgres://aedst:aedst@localhost:5432/aedst?sslmode=disable

## dev: start local infra, run migrations, run the server.
dev: up migrate-up
	go run ./cmd/server

## up: start Postgres/Valkey for local development.
up:
	$(COMPOSE) up -d --wait

## down: stop local infra.
down:
	$(COMPOSE) down

## generate: regenerate all generated code (proto + sqlc). Must be re-run
## (and the diff committed) after any .proto or sql/ change.
generate:
	buf generate
	sqlc generate

## lint: format check, vet, golangci-lint, buf lint/format, architecture tests.
## Does not include `breaking` — that needs network access to diff against
## origin/main and is its own target, run separately (also wired into CI on
## every PR).
lint:
	gofumpt -l .
	go vet ./...
	golangci-lint run ./...
	buf lint
	buf format -d --exit-code
	go test ./architecturetest/...

## breaking: fail if this branch's proto changes break wire/source
## compatibility against main. Run before pushing a proto change — CI runs
## the same check on every PR.
breaking:
	buf breaking --against "../../.git#branch=main,subdir=apps/server"

## test: unit tests only (no external services required).
test:
	go test ./internal/... ./cmd/...

## integration-test: tests that talk to real Postgres/Valkey via
## Testcontainers. Requires Docker.
integration-test:
	go test -tags=integration ./...

## security: dependency vulnerability scan.
security:
	govulncheck ./...

## build: compile the server binary.
build:
	go build -o bin/aedst-server ./cmd/server

## migrate-up: apply all pending migrations for every module schema.
migrate-up:
	for m in auth tasks notifications; do \
		goose -dir migrations/$$m -table goose_$$m postgres "$(DB_URL)" up; \
	done

## migrate-down: roll back the latest migration for every module schema.
migrate-down:
	for m in auth tasks notifications; do \
		goose -dir migrations/$$m -table goose_$$m postgres "$(DB_URL)" down; \
	done

clean:
	rm -rf bin
