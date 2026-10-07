SHELL := /bin/bash
SERVER_PROTO_DIR := ../server/api/proto
GENERATED_DIR := Core/AedstNetwork/Sources/AedstNetwork/Generated

.PHONY: generate
generate: ## Generate Swift Connect client stubs from apps/server's proto sources (see buf.gen.yaml).
	mkdir -p $(GENERATED_DIR)
	# Delete only generated sources: Generated/README.md is tracked (and excluded in Package.swift).
	find $(GENERATED_DIR) -name "*.swift" -delete
	buf generate $(SERVER_PROTO_DIR) \
		--template buf.gen.yaml \
		--path $(SERVER_PROTO_DIR)/aedst/auth/v1/auth.proto \
		--path $(SERVER_PROTO_DIR)/aedst/tasks/v1/tasks.proto \
		--path $(SERVER_PROTO_DIR)/aedst/notifications/v1/notifications.proto \
		--path $(SERVER_PROTO_DIR)/aedst/search/v1/search.proto \
		--path $(SERVER_PROTO_DIR)/aedst/media/v1/media.proto \
		--path $(SERVER_PROTO_DIR)/aedst/admin/v1/admin.proto
	buf generate buf.build/bufbuild/protovalidate \
		--template buf.gen.yaml \
		--path buf/validate/validate.proto
	@echo "Generated Connect-Swift stubs into $(GENERATED_DIR)"


.PHONY: build
build: generate ## swift build every local package (does not build the .xcodeproj app targets).
	for pkg in Core/* Features/* App/Shared; do \
		echo "==> swift build ($$pkg)"; \
		(cd $$pkg && swift build) || exit 1; \
	done

.PHONY: test
test: generate ## swift test every local package.
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

.PHONY: install
install: ## Resolve third-party/local Swift packages for Tuist (Tuist/Package.swift).
	tuist install

.PHONY: project
project: install ## Generate AEDST.xcodeproj / AEDST.xcworkspace with Tuist (both gitignored).
	tuist generate --no-open

.PHONY: bootstrap
bootstrap: generate project ## One-shot setup after cloning: Connect codegen + Tuist install + Xcode project.

.PHONY: tuist-build
tuist-build: ## Build both app schemes through Tuist (requires Xcode).
	tuist build App_iOS
	tuist build App_macOS

.PHONY: tuist-test
tuist-test: ## Run the app schemes' tests through Tuist (package unit tests run via `make test`).
	tuist test App_iOS
	tuist test App_macOS

.PHONY: release-ios
release-ios: ## Archive + export (UPLOAD=1 to send to TestFlight) the iOS app. Needs APPLE_TEAM_ID; see docs/content/apple-release.md.
	bash scripts/release-ios.sh

.PHONY: release-macos
release-macos: ## Archive, export, notarize and staple the macOS app (DISTRIBUTION=app-store for the Mac App Store).
	bash scripts/release-macos.sh
