#!/usr/bin/env bash
# One entry point for the design-system pixel snapshots (Core/AedstDesignSystem). macOS + Xcode only.
#
#   scripts/snapshots.sh record   Re-record EVERY reference (iOS simulator + macOS), then verify them once.
#   scripts/snapshots.sh verify   Check against the recorded references. With no references for a platform it
#                                 skips that platform with a notice (exit 0); with references a diff fails.
#   scripts/snapshots.sh status   Print what is recorded and the environment it was recorded on.
#
# Why this exists: references are rasterizer-specific, so they must be recorded on the same Xcode / simulator
# runtime / macOS family that checks them, and they cannot be produced on a machine without Xcode. CI records
# them in the manual workflow .github/workflows/apple-record-snapshots.yml, which calls `record` here.
# Documented in docs/content/apple/snapshots.md.
#
# UNVERIFIED: written without a Swift toolchain, never run.
set -euo pipefail

cd "$(dirname "$0")/.."

PACKAGE="Core/AedstDesignSystem"
SNAP_DIR="$PACKAGE/Tests/AedstDesignSystemTests/Snapshots/__Snapshots__"
REF_DIR="$SNAP_DIR/ComponentSnapshotTests"
ENV_FILE="$SNAP_DIR/ENVIRONMENT.txt"

# The pinned iOS Simulator. UNVERIFIED default: confirm with `xcrun simctl list runtimes` on the runner image
# and change it here (one place; both workflows read this script).
SIM_DEVICE="${SNAPSHOT_SIM_DEVICE:-iPhone 17}"
SIM_OS="${SNAPSHOT_SIM_OS:-26.6}"
# SwiftPM's generated scheme for the package; UNVERIFIED, check with `xcodebuild -list` in $PACKAGE.
SCHEME="${SNAPSHOT_SCHEME:-AedstDesignSystem-Package}"
RESULTS="${SNAPSHOT_RESULTS_DIR:-${RUNNER_TEMP:-/tmp}/aedst-snapshots}"

mkdir -p "$RESULTS"

die() {
  echo "::error::$*" >&2
  exit 1
}

require_xcode() {
  command -v xcodebuild >/dev/null 2>&1 || die "xcodebuild not found: snapshots need macOS with Xcode"
}

has_references() { # ios | macos
  [ -d "$REF_DIR" ] || return 1
  case "$1" in
    ios) ls "$REF_DIR" | grep -Eq '(iphone|ipad)-.*\.png$' ;;
    macos) ls "$REF_DIR" | grep -Eq 'mac-.*\.png$' ;;
  esac
}

destination() { # ios | macos
  case "$1" in
    ios) echo "platform=iOS Simulator,name=$SIM_DEVICE,OS=$SIM_OS" ;;
    macos) echo "platform=macOS" ;;
  esac
}

# The environment a recording is only valid for: compared by `verify` so a drifted runner image produces one
# clear message instead of dozens of mysterious pixel diffs.
current_environment() {
  echo "xcode=$(xcodebuild -version | head -n 1)"
  echo "ios_simulator=$SIM_DEVICE iOS $SIM_OS"
  echo "macos_major=$(sw_vers -productVersion | cut -d. -f1)"
}

check_environment() {
  [ -f "$ENV_FILE" ] || die "$ENV_FILE is missing: references without a recorded environment cannot be trusted; re-record"
  if ! diff <(sort "$ENV_FILE") <(current_environment | sort) >"$RESULTS/env.diff"; then
    cat "$RESULTS/env.diff"
    die "references were recorded on a different environment (left: recorded, right: now). Re-record with the apple-record-snapshots workflow"
  fi
}

ensure_simulator() {
  if ! xcrun simctl list devices available | awk -v header="-- iOS $SIM_OS --" '$0 == header {on = 1; next} /^-- / {on = 0} on' | grep -q "$SIM_DEVICE ("; then
    xcrun simctl list runtimes >&2
    die "no available simulator '$SIM_DEVICE' on iOS $SIM_OS; fix SNAPSHOT_SIM_OS / SNAPSHOT_SIM_DEVICE in scripts/snapshots.sh"
  fi
}

# run_tests <ios|macos> <record|verify>
run_tests() {
  local platform="$1" mode="$2" record_value="never" bundle="$RESULTS/$1-$2.xcresult"
  [ "$mode" = "record" ] && record_value="all"
  rm -rf "$bundle"
  echo "==> snapshots: $platform $mode ($(destination "$platform"))"
  (
    cd "$PACKAGE"
    # xcodebuild forwards TEST_RUNNER_* variables to the test process with the prefix stripped.
    TEST_RUNNER_AEDST_SNAPSHOTS="$mode" TEST_RUNNER_SNAPSHOT_TESTING_RECORD="$record_value" \
      xcodebuild test \
        -scheme "$SCHEME" \
        -destination "$(destination "$platform")" \
        -resultBundlePath "$bundle"
  )
}

verify() {
  require_xcode
  local ran=0
  for platform in ios macos; do
    if has_references "$platform"; then
      check_environment
      [ "$platform" = "ios" ] && ensure_simulator
      run_tests "$platform" verify
      ran=1
    else
      echo "::notice::snapshots: no $platform references recorded; skipping (run the apple-record-snapshots workflow or 'scripts/snapshots.sh record' on a Mac)"
    fi
  done
  [ "$ran" -eq 1 ] || echo "snapshots: nothing to verify"
}

record() {
  require_xcode
  ensure_simulator
  # Start clean: a renamed or deleted test must not leave a stale reference behind.
  rm -rf "$REF_DIR"
  mkdir -p "$SNAP_DIR"
  # A recording run reports every written image as an issue ("re-run to assert"), so its exit code is
  # expected to be non-zero. The verify pass below is what proves the recording is complete and stable.
  run_tests ios record || echo "(recording run reported the expected 'recorded' issues)"
  run_tests macos record || echo "(recording run reported the expected 'recorded' issues)"
  has_references ios || die "the iOS recording run wrote no references"
  has_references macos || die "the macOS recording run wrote no references"
  current_environment >"$ENV_FILE"
  echo "==> verifying the fresh recording (must pass without writing anything)"
  run_tests ios verify
  run_tests macos verify
  echo "Recorded $(ls "$REF_DIR" | wc -l | tr -d ' ') reference images in $REF_DIR"
}

status() {
  echo "references dir: $REF_DIR"
  for platform in ios macos; do
    if has_references "$platform"; then
      echo "$platform: $(ls "$REF_DIR" | grep -Ec "$([ "$platform" = ios ] && echo '(iphone|ipad)-' || echo 'mac-')") image(s)"
    else
      echo "$platform: none recorded"
    fi
  done
  if [ -f "$ENV_FILE" ]; then
    echo "recorded on:"
    sed 's/^/  /' "$ENV_FILE"
  fi
}

case "${1:-}" in
  record) record ;;
  verify) verify ;;
  status) status ;;
  *)
    echo "usage: $0 record|verify|status" >&2
    exit 2
    ;;
esac
