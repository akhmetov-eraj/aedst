#!/usr/bin/env bash
# qmllint over every QML file in App/ and Modules/ (blocking).
#
#   ./scripts/check-qml-lint.sh                 # lint (what CI runs)
#   ./scripts/check-qml-lint.sh --format        # also verify qmlformat is a no-op
#   QML_IMPORT_PATH=<paths>  extra import paths, ':'-separated (module QML dirs)
#
# qmllint resolves imports through qmltypes/qmldir files: after a build the
# generated ones live in build/<preset>/ (pass BUILD_DIR=build/gcc-debug so the
# design-system and app modules are found). Without them qmllint reports
# "module not found" for first-party imports; in that no-build run only the import category is downgraded
# (--import info, below), everything else stays an error. With BUILD_DIR set the generated modules resolve, so an
# unresolved import is an ERROR there (--import error): that is the run that proves the QML module graph.
#
# Needs qmllint (and qmlformat for --format) from the Qt install: $QT_ROOT_DIR/bin
# or on PATH (also qmllint-qt6 on some distros).
set -euo pipefail

cd "$(dirname "$0")/.."

find_tool() {
  local name="$1" p
  for p in "${QT_ROOT_DIR:-}/bin/$name" "${QT_ROOT_DIR:-}/bin/$name.exe"; do
    [ -x "$p" ] && { echo "$p"; return; }
  done
  command -v "$name" || command -v "$name-qt6" || true
}

qmllint=$(find_tool qmllint)
if [ -z "$qmllint" ]; then
  echo "error: qmllint not found (set QT_ROOT_DIR to the Qt install, or put qmllint on PATH)." >&2
  exit 127
fi

files=$(find App Modules -type f -name '*.qml' -not -path '*/build/*')
if [ -z "$files" ]; then
  echo "no QML files found"; exit 0
fi

args=()
if [ -n "${BUILD_DIR:-}" ]; then
  args+=(--import error)
  # Generated qmldir/.qmltypes live under each module's binary dir, in <URI path>/ below it:
  # the import root is the directory that CONTAINS that <URI path> directory.
  args+=(-I "$BUILD_DIR" -I "$BUILD_DIR/qml")
  for p in "$BUILD_DIR/Modules/Core/DesignSystem" "$BUILD_DIR/App"; do
    [ -d "$p" ] && args+=(-I "$p")
  done
fi
if [ -n "${QT_ROOT_DIR:-}" ] && [ -d "$QT_ROOT_DIR/qml" ]; then
  args+=(-I "$QT_ROOT_DIR/qml")
fi
# No build tree: first-party modules cannot resolve, so only the "import" category is informational.
[ -z "${BUILD_DIR:-}" ] && args+=(--import info)
IFS=':' read -r -a extra <<< "${QML_IMPORT_PATH:-}"
for p in "${extra[@]:-}"; do [ -n "$p" ] && args+=(-I "$p"); done

# xargs -0 so paths with spaces survive; qmllint exits non-zero on any error-level message.
echo "$files" | tr '\n' '\0' | xargs -0 "$qmllint" ${args[@]+"${args[@]}"}
echo "OK: qmllint clean ($(echo "$files" | wc -l | tr -d ' ') files)"

if [ "${1:-}" = "--format" ]; then
  qmlformat=$(find_tool qmlformat)
  if [ -z "$qmlformat" ]; then
    echo "error: qmlformat not found." >&2
    exit 127
  fi
  bad=0
  while IFS= read -r f; do
    if ! diff -q <("$qmlformat" "$f") "$f" >/dev/null; then
      echo "not qmlformat-clean: $f"
      bad=1
    fi
  done <<< "$files"
  [ "$bad" -eq 0 ] && echo "OK: qmlformat clean" || exit 1
fi
