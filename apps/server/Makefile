.PHONY: dev down generate lint test integration-test security build clean migrate-up migrate-down

COMPOSE := docker compose -f deployments/docker/docker-compose.yml
DB_URL ?= postgres://aedst:aedst@localhost:5432/aedst?sslmode=disable

## dev: start local infra, run migrations, run the server.
dev: up migrate-up
	go run ./cmd/server

## up: start Postgres/Redis/NATS for local development.
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

## lint: format check, vet, golangci-lint, buf lint/breaking, architecture tests.
lint:
	gofumpt -l .
	go vet ./...
	golangci-lint run ./...
	buf lint
	go test ./architecturetest/...

## test: unit tests only (no external services required).
test:
	go test ./internal/... ./cmd/...

## integration-test: tests that talk to real Postgres/Redis/NATS via
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
	for m in auth users profile; do \
		goose -dir migrations/$$m postgres "$(DB_URL)" up; \
	done

## migrate-down: roll back the latest migration for every module schema.
migrate-down:
	for m in auth users profile; do \
		goose -dir migrations/$$m postgres "$(DB_URL)" down; \
	done

clean:
	rm -rf bin
