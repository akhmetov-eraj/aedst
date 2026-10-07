# Qt GRPC / Qt Protobuf: what a move would bring (note, not a decision)

Status: **not adopted**. Qt Protobuf and Qt GRPC are available under GPLv3 or a commercial license from The Qt
Company (Qt documentation, checked for 6.12). AEDST Desktop is not GPL, and nobody has confirmed a commercial
license, so [ADR-0001](adr/0001-api-contract-client.md) keeps the hand-written `ConnectClient` over Qt Network and
build-time `protoc` messages. This note is the material for the license decision; it changes nothing by itself.

**Decision needed from the owner (irreversible, money):** buying a commercial Qt license (or choosing GPLv3
for the whole client). Until then, do not add `Qt6::Protobuf` / `Qt6::Grpc` to any target.

## What would change

| Topic | Today (`ConnectClient` + `protoc`) | With Qt GRPC / Qt Protobuf |
|---|---|---|
| Messages | `protoc`-generated C++ (`google::protobuf`), full runtime from vcpkg | `qtprotobufgen` (`qt_add_protobuf`): `QProtobufMessage` types with Qt types (`QString`, `QDateTime`, ...) and QML exposure |
| Service stubs | Hand-written table (`contract/methods.hpp`) kept in sync by descriptor tests | `qtgrpcgen` (`qt_add_grpc`) generates the client from the same `.proto`; the drift tests become unnecessary |
| Protocol | Connect over HTTP/1.1 or HTTP/2 (works through proxies and CDNs that break gRPC trailers) | gRPC over HTTP/2. The server also speaks gRPC, so it works, but HTTP/2 end to end becomes a requirement (corporate proxies, some CDNs/LBs) |
| Direction | Unary + server streaming | Qt GRPC is a client-side API. Call types beyond ours (client/bidi streaming) are to be confirmed against the 6.12 API reference before relying on them |
| Our policy code | Per-attempt deadline, idempotent-only retry with jittered backoff, single-flight token refresh, SPKI pinning, correlation ids, `std::expected` errors | None of it comes for free. Deadlines, metadata and TLS configuration exist in Qt GRPC (API to be verified); retry, refresh-and-replay, pinning and the error model would be re-implemented around its signal-based API |
| Threading and API style | Completion callbacks on the owner thread | Signals/slots and QML friendly; convenient in QML, heavier in the Qt-free Domain code this project keeps free of Qt |
| Dependencies | `protobuf`, `abseil` from vcpkg | Qt modules for the API layer. Protovalidate descriptors would still need protobuf or a replacement |
| Maintenance | We maintain ~700 lines of client | Qt maintains it; we follow Qt releases |

## What would NOT change

- The server contract and `buf` checks. Protovalidate rules stay a server concern.
- The OIDC flow, secret storage and the local database.
- The need for a policy layer (refresh, retries, error mapping) on top.

## Recommendation

Do not switch unless the commercial license is bought for other reasons. The gain is smaller than it looks (stubs
and message types) because the valuable part of `ConnectClient` is policy that Qt GRPC does not provide. If the
license is bought, migrate behind the existing seams: Domain keeps `core::Outcome`, Data repositories keep their
interfaces, and only `ConnectClient`/`Contract` change. Re-run the Phase 5 test suite against the new client as the
acceptance test. Items marked "to be confirmed" above must be checked against the Qt 6.12 API reference first.
