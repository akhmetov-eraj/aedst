SHELL := /bin/bash
SERVER_PROTO_DIR := ../server/api/proto
GENERATED_DIR := core/network/src/main/kotlin-gen

.PHONY: generate
generate: ## Generate Kotlin ConnectRPC/Protobuf client stubs from apps/server's proto sources (see buf.gen.yaml).
	rm -rf $(GENERATED_DIR)
	mkdir -p $(GENERATED_DIR)
	buf generate $(SERVER_PROTO_DIR) \
		--template buf.gen.yaml \
		--path $(SERVER_PROTO_DIR)/aedst/auth/v1/auth.proto \
		--path $(SERVER_PROTO_DIR)/aedst/tasks/v1/tasks.proto \
		--path $(SERVER_PROTO_DIR)/aedst/media/v1/media.proto \
		--path $(SERVER_PROTO_DIR)/aedst/notifications/v1/notifications.proto \
		--path $(SERVER_PROTO_DIR)/aedst/search/v1/search.proto \
		--path $(SERVER_PROTO_DIR)/aedst/admin/v1/admin.proto
	@echo "Generated ConnectRPC/Protobuf Kotlin stubs into $(GENERATED_DIR)"

.PHONY: bootstrap
bootstrap: generate ## One-shot setup after cloning: codegen + placeholder google-services.json, then open the project in Android Studio.
	@test -f app/google-services.json || cp app/google-services.json.example app/google-services.json

.PHONY: check
check: generate ## The same gates as CI (lint job + unit tests): ktlint, detekt, Android Lint, JVM tests.
	./gradlew ktlintCheck detekt lintDebug testDebugUnitTest
	./scripts/check-feature-boundaries.sh

.PHONY: format
format: ## Auto-format Kotlin sources with ktlint.
	./gradlew ktlintFormat

.PHONY: connected
connected: ## Compose UI tests with Accessibility Test Framework checks (needs an emulator/device).
	./gradlew connectedDebugAndroidTest
