#!/usr/bin/env bash
# Fails if an RPC declared in apps/server/api/proto has no entry in
# Modules/Core/Contract/include/aedst/contract/methods.hpp (and vice versa).
#
# The desktop generates its message types from the protos at build time
# (cmake/AedstProtobuf.cmake), so nothing can go stale there. The one
# hand-written artefact is the procedure table (path + auth + idempotency); the
# Contract unit test checks it against the generated descriptors, and this
# script gives the same answer in seconds in the lint job, before any build.
#
# The optional argument (a git diff range, passed by older CI configs) is ignored.
set -euo pipefail

cd "$(dirname "$0")/.."

proto_root=../server/api/proto
table=Modules/Core/Contract/include/aedst/contract/methods.hpp
streaming_table=Modules/Core/Contract/include/aedst/contract/streaming_methods.hpp

# Services the desktop client never calls: device attestation is an iOS (App Attest) and Android (Play Integrity)
# concern; there is no desktop equivalent. Keep in step with kIgnoredProtoFiles in
# Modules/Core/Contract/tests/methods_test.cpp.
ignored_services='^/aedst.(appattest|integrity).v1.'

# "/<package>.<Service>/<Method>" for every rpc in every proto.
declared=$(find "$proto_root" -name '*.proto' -print0 | xargs -0 awk '
  /^package[[:space:]]/ { gsub(/;/, "", $2); pkg = $2 }
  /^service[[:space:]]/ { svc = $2 }
  /^[[:space:]]*rpc[[:space:]]/ {
    name = $2; sub(/\(.*/, "", name)
    print "/" pkg "." svc "/" name
  }' | grep -Ev "$ignored_services" | sort -u)

# Paths listed in the tables (unary and server-streaming). The streaming table names its RPC only inside
# `#if AEDST_CONTRACT_HAS_WATCH_NOTIFICATIONS`; the entry is the answer to "was the new RPC handled", so it is
# only expected once the server proto declares the RPC (see the filtering of `extra` below).
listed=$(grep -ohE '"/[A-Za-z0-9_.]+/[A-Za-z0-9_]+"' "$table" "$streaming_table" | tr -d '"' | sort -u)

status=0
missing=$(comm -23 <(echo "$declared") <(echo "$listed") || true)
extra=$(comm -13 <(echo "$declared") <(echo "$listed") | grep -v 'NotificationsService/WatchNotifications' || true)

if [ -n "$missing" ]; then
  echo "::error::RPCs in apps/server/api/proto with no entry in $table:"
  echo "$missing"
  status=1
fi
if [ -n "$extra" ]; then
  echo "::error::$table lists RPCs that apps/server/api/proto no longer declares:"
  echo "$extra"
  status=1
fi

if [ "$status" -eq 0 ]; then
  echo "OK: $table matches the RPCs declared in $proto_root ($(echo "$declared" | wc -l | tr -d ' ') methods)."
fi
exit "$status"
