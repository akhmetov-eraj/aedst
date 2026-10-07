#!/usr/bin/env bash
# Enforces the C++23 subset documented in CODING_STANDARDS.md ("Allowed C++23 subset").
#
# Fails when first-party code (Modules/, App/, Tests/) uses a library or language
# feature that is NOT available in all of: MSVC 19.37, GCC 13 + libstdc++ 13,
# Clang 17 + libstdc++ 13. The support table is from the compilers' published
# conformance pages and is UNVERIFIED by a real build (nothing has been compiled yet):
# when CI first runs, fix this list from the evidence, not the other way round.
#
# Comments are not stripped on purpose (a banned name in a comment is still a smell),
# except lines containing the marker `cxx-subset: allow` for documented exceptions.
set -uo pipefail
cd "$(dirname "$0")/.."

banned='std::(print|println|flat_map|flat_set|flat_multimap|flat_multiset|generator|stacktrace|mdspan|spanstream|out_ptr|inout_ptr|ranges::to|ranges::fold_[a-z_]+|views::(zip|zip_transform|enumerate|chunk|chunk_by|slide|stride|join_with|cartesian_product|adjacent|pairwise|as_rvalue|repeat)|format_string|runtime_format|basic_format_string|bind_back|ranges::contains|ranges::starts_with|ranges::ends_with|ranges::shift_[a-z]+|unreachable_sentinel_t)\b|import +std|\(this +(auto|const|[A-Za-z_:]+)[ &]+self\b|\[\[assume\(|\bstd::format\('

status=0
hits=$(grep -rnE "$banned" Modules App Tests --include=*.cpp --include=*.hpp --include=*.h \
        | grep -v 'cxx-subset: allow' || true)
if [ -n "$hits" ]; then
  echo "::error::C++ features outside the allowed subset (see CODING_STANDARDS.md):"
  echo "$hits"
  status=1
fi

# The feature probe in cmake/AedstCompilerFeatures.cmake must stay in step with the doc.
if ! grep -q 'Allowed C++23 subset' CODING_STANDARDS.md; then
  echo "::error::CODING_STANDARDS.md lost its 'Allowed C++23 subset' section."
  status=1
fi

[ "$status" -eq 0 ] && echo "OK: C++ subset check passed."
exit "$status"
