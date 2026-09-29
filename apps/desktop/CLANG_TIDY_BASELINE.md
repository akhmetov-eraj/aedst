# clang-tidy baseline

`clang-tidy` runs in `.github/workflows/desktop-ci.yml`'s `test` job with
`continue-on-error: true` because the tree has never had a real clang-tidy
pass — the check exists so warnings are visible (see the uploaded
`clang-tidy-log` artifact on each run), but a pre-existing warning doesn't
fail CI yet.

## Flip condition

Set `continue-on-error: false` once `run-clang-tidy -p build/debug -quiet
'Modules/|App/'` exits 0 against `main` — i.e. the current warning backlog
(check the latest `clang-tidy-log` artifact) is cleared or explicitly
suppressed via `.clang-tidy`'s `Checks:`/`--line-filter`, not just ignored.

Until then, a PR that adds new clang-tidy warnings on top of the existing
backlog won't be blocked by CI — reviewers should still treat new warnings
in the diff as a review comment.
