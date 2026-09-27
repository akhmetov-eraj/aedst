#!/usr/bin/env node
// Generates the Connect-ES TypeScript client stubs used by
// `src/shared/api/connect/client.ts` straight from apps/server's proto
// sources (apps/server/api/proto is the single source of truth — nothing is
// duplicated in apps/web). See buf.gen.yaml for plugin config.
//
// protoc-gen-es alone is enough: Connect-ES v2's unified codegen has
// protoc-gen-es emit each service's `GenService` descriptor (via
// @bufbuild/protobuf/codegenv2) right alongside its message types, so
// `@connectrpc/connect`'s `createClient()` consumes it directly — there is
// no separate protoc-gen-connect-es plugin/output for this codegen style.
//
// Two `buf generate` invocations are required:
//   1. The three RPC-bearing protos from apps/server (auth, users, profile).
//      `events.proto` files are deliberately excluded — those describe NATS
//      messages, not RPCs, and have no business in a browser client.
//   2. `buf/validate/validate.proto`, the protovalidate dependency that (1)'s
//      generated code imports for field-constraint descriptors. protoc-gen-es
//      only emits code for files it's explicitly told to generate, so this
//      dependency needs its own pass into the same output directory.
import { rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const rootDir = dirname(dirname(fileURLToPath(import.meta.url)));
const serverProtoDir = join(rootDir, "..", "server", "api", "proto");
const generatedDir = join(rootDir, "src", "shared", "api", "connect", "generated");
const binDir = join(rootDir, "node_modules", ".bin");

const env = {
  ...process.env,
  PATH: `${binDir}${process.platform === "win32" ? ";" : ":"}${process.env.PATH ?? ""}`,
};

function buf(args) {
  const bufBin = join(binDir, process.platform === "win32" ? "buf.CMD" : "buf");
  // shell: true is required on Windows to execute the pnpm-generated .CMD
  // shim; args are passed as an array (not a single interpolated string) so
  // they are still safely quoted rather than naively concatenated.
  const result = spawnSync(bufBin, args, {
    cwd: rootDir,
    env,
    stdio: "inherit",
    shell: true,
  });
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

rmSync(generatedDir, { recursive: true, force: true });

buf([
  "generate",
  serverProtoDir,
  "--path",
  join(serverProtoDir, "aedst", "auth", "v1", "auth.proto"),
  "--path",
  join(serverProtoDir, "aedst", "users", "v1", "users.proto"),
  "--path",
  join(serverProtoDir, "aedst", "profile", "v1", "profile.proto"),
]);

buf(["generate", "buf.build/bufbuild/protovalidate", "--path", "buf/validate/validate.proto"]);

console.log("Connect-ES stubs generated into src/shared/api/connect/generated");
