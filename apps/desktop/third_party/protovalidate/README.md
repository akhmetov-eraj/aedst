# Vendored: protovalidate `buf/validate/validate.proto`

The server's protos annotate fields with `(buf.validate.field)` rules, so protoc
must be able to resolve `import "buf/validate/validate.proto"` to generate the
desktop's C++ messages. That file normally comes from the Buf Schema Registry
(`buf.build/bufbuild/protovalidate`, pinned by `apps/server/buf.lock`). It is
vendored here so a desktop build needs neither `buf` nor network access.

- Source: `buf export buf.build/bufbuild/protovalidate:511051f7f4374c3ca873b53ae68a9288`
  (the commit in `apps/server/buf.lock`)
- License: Apache-2.0 (header retained in the file), (c) Buf Technologies, Inc.
- Used for: compiling the annotated protos only. The desktop does not evaluate
  the rules - the server validates every request - so nothing links a
  protovalidate runtime.

Update procedure when `apps/server/buf.lock` moves:

```bash
COMMIT=<new commit from buf.lock>
buf export "buf.build/bufbuild/protovalidate:$COMMIT" -o apps/desktop/third_party/protovalidate
# then update the commit above
```

`scripts/check-protovalidate-pin.sh` (run in CI where `buf` is available)
fails when this file no longer matches the commit in `buf.lock`.
