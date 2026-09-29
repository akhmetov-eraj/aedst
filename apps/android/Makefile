SHELL := /bin/bash
SERVER_PROTO_DIR := ../server/api/proto
GENERATED_DIR := core/network/src/main/kotlin-gen

.PHONY: generate
generate: ## Generate Kotlin ConnectRPC/Protobuf client stubs from apps/server's proto sources (see buf.gen.yaml).
	rm -rf $(GENERATED_DIR)
	mkdir -p $(GENERATED_DIR)
	buf generate $(SERVER_PROTO_DIR) \
		--template buf.gen.yaml \
		--path $(SERVER_PROTO_DIR)/aedst/auth/v1/auth.proto
	@echo "Generated ConnectRPC/Protobuf Kotlin stubs into $(GENERATED_DIR)"

.PHONY: bootstrap
bootstrap: generate ## One-shot setup after cloning: codegen, then open the project in Android Studio.
