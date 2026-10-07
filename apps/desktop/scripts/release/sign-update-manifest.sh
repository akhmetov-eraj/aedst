#!/usr/bin/env bash
# Builds and Ed25519-signs the update manifest the desktop app verifies (Aedst::Update, ADR-0029).
#
#   scripts/release/sign-update-manifest.sh \
#       --version 1.6.0 --channel stable --min-supported 1.2.0 \
#       --notes-url https://aedst.com/releases/1.6.0 \
#       --artifact windows:x86_64:https://downloads.aedst.com/1.6.0/AEDST-1.6.0-Setup.exe:dist/AEDST-1.6.0-Setup.exe \
#       --artifact linux:x86_64:https://downloads.aedst.com/1.6.0/AEDST-1.6.0.AppImage:dist/AEDST-1.6.0.AppImage \
#       --key-id k1 --out dist/stable.json
#
#   Add --rollback to publish a signed rollback (the app installs it even though it is older than the installed version
#   and treats it as mandatory). --published-at must grow with every release of a channel (replay protection); it defaults
#   to the current time.
#
# The private key (PEM, Ed25519) comes from $AEDST_UPDATE_SIGNING_KEY (the PEM text, for CI secrets) or from the file named
# by $AEDST_UPDATE_SIGNING_KEY_FILE. It is never accepted as an argument and never printed. The envelope signs the exact
# bytes of the payload (no canonicalisation): { "payload": base64(manifest), "signatures": [{ "keyId", "signature" }] }.
#
# Requires: bash, jq, openssl >= 3.0 (pkeyutl -rawin), sha256sum, base64.
set -euo pipefail

usage() {
  sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-2}"
}

version="" channel="stable" min_supported="" notes_url="" key_id="" out="" rollback="false"
published_at="$(date -u +%s)"
artifacts=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version) version="$2"; shift 2 ;;
    --channel) channel="$2"; shift 2 ;;
    --min-supported) min_supported="$2"; shift 2 ;;
    --notes-url) notes_url="$2"; shift 2 ;;
    --published-at) published_at="$2"; shift 2 ;;
    --artifact) artifacts+=("$2"); shift 2 ;;
    --key-id) key_id="$2"; shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --rollback) rollback="true"; shift ;;
    -h|--help) usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage ;;
  esac
done

[[ -n "$version" && -n "$min_supported" && -n "$notes_url" && -n "$key_id" && -n "$out" && ${#artifacts[@]} -gt 0 ]] || usage
[[ "$channel" == "stable" || "$channel" == "beta" ]] || { echo "channel must be stable or beta" >&2; exit 2; }
[[ "$published_at" =~ ^[0-9]+$ ]] || { echo "--published-at must be unix seconds" >&2; exit 2; }
[[ "$key_id" =~ ^[A-Za-z0-9._-]{1,32}$ ]] || { echo "key id: 1..32 chars of [A-Za-z0-9._-]" >&2; exit 2; }

for tool in jq openssl sha256sum base64; do
  command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
umask 077

if [[ -n "${AEDST_UPDATE_SIGNING_KEY:-}" ]]; then
  printf '%s\n' "$AEDST_UPDATE_SIGNING_KEY" > "$work/key.pem"
elif [[ -n "${AEDST_UPDATE_SIGNING_KEY_FILE:-}" ]]; then
  cp "$AEDST_UPDATE_SIGNING_KEY_FILE" "$work/key.pem"
else
  echo "set AEDST_UPDATE_SIGNING_KEY (PEM) or AEDST_UPDATE_SIGNING_KEY_FILE" >&2
  exit 1
fi

# platform:arch:url:file  (the url contains ':' so split the two ends and keep the middle)
artifacts_json="[]"
for spec in "${artifacts[@]}"; do
  platform="${spec%%:*}"; rest="${spec#*:}"
  arch="${rest%%:*}"; rest="${rest#*:}"
  file="${rest##*:}"; url="${rest%:*}"
  [[ -f "$file" ]] || { echo "artifact file not found: $file" >&2; exit 1; }
  [[ "$url" == https://* ]] || { echo "artifact url must be https: $url" >&2; exit 1; }
  sha="$(sha256sum "$file" | cut -d' ' -f1)"
  size="$(wc -c < "$file" | tr -d ' ')"
  artifacts_json="$(jq -c --arg p "$platform" --arg a "$arch" --arg u "$url" --arg s "$sha" --argjson z "$size" \
    '. + [{platform:$p, arch:$a, url:$u, sha256:$s, size:$z}]' <<<"$artifacts_json")"
done

jq -cn --arg v "$version" --arg c "$channel" --arg n "$notes_url" --arg m "$min_supported" \
  --argjson t "$published_at" --argjson r "$rollback" --argjson a "$artifacts_json" \
  '{version:$v, channel:$c, releaseNotesUrl:$n, minSupportedVersion:$m, publishedAt:$t, artifacts:$a}
   + (if $r then {rollback:true} else {} end)' | tr -d '\n' > "$work/payload.json"

openssl pkeyutl -sign -rawin -inkey "$work/key.pem" -in "$work/payload.json" -out "$work/sig.bin"
[[ "$(wc -c < "$work/sig.bin" | tr -d ' ')" == "64" ]] || { echo "key is not an Ed25519 key (signature is not 64 bytes)" >&2; exit 1; }

jq -n --arg p "$(base64 -w0 < "$work/payload.json")" --arg k "$key_id" --arg s "$(base64 -w0 < "$work/sig.bin")" \
  '{payload:$p, signatures:[{keyId:$k, signature:$s}]}' > "$out"

echo "signed $channel manifest for $version -> $out (key $key_id, published-at $published_at)"
