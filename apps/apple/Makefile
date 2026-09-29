SHELL := /bin/bash
SERVER_PROTO_DIR := ../server/api/proto
GENERATED_DIR := Core/AedstNetwork/Sources/AedstNetwork/Generated

.PHONY: generate
generate: ## Generate Swift Connect client stubs from apps/server's proto sources (see buf.gen.yaml).
	rm -rf $(GENERATED_DIR)
	mkdir -p $(GENERATED_DIR)
	buf generate $(SERVER_PROTO_DIR) \
		--template buf.gen.yaml \
		--path $(SERVER_PROTO_DIR)/aedst/auth/v1/auth.proto \
		--path $(SERVER_PROTO_DIR)/aedst/tasks/v1/tasks.proto
	buf generate buf.build/bufbuild/protovalidate \
		--template buf.gen.yaml \
		--path buf/validate/validate.proto
	@echo "Generated Connect-Swift stubs into $(GENERATED_DIR)"

.PHONY: project
project: ## Regenerate AEDST.xcodeproj from project.yml (requires xcodegen).
	xcodegen generate

.PHONY: bootstrap
bootstrap: generate project ## One-shot setup after cloning: codegen + Xcode project.

.PHONY: build
build: ## swift build every local package (does not build the .xcodeproj app targets).
	for pkg in Core/* Features/* App/Shared; do \
		echo "==> swift build ($$pkg)"; \
		(cd $$pkg && swift build) || exit 1; \
	done

.PHONY: test
test: ## swift test every local package.
	for pkg in Core/* Features/* App/Shared; do \
		echo "==> swift test ($$pkg)"; \
		(cd $$pkg && swift test) || exit 1; \
	done

.PHONY: format
format: ## swift-format --in-place every package (SwiftFormat CLI, see .swiftformat).
	swift format --in-place --recursive Core Features App

.PHONY: lint
lint: ## swiftlint across every package.
	swiftlint lint --config .swiftlint.yml
