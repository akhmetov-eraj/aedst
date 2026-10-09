# GitHub setup checklist (one-time, per cloned product)

These settings live on GitHub, not in the tree, so no check can enforce them. Do them right after the first push.

| Setting | Where | Why |
|---|---|---|
| Default branch `main` | `git branch -m master main` before the first push | Workflows and `task breaking` (`apps/server/Taskfile.yml`) expect `main` |
| Template repository | Settings > General > Template repository | Lets `gh repo create <name> --template <owner>/aedst` work; then run `task template:new` |
| Dependency graph | Settings > Security > Code security | `dependency-review` job in `web-ci.yml` fails without it |
| Environment `ai-task` | Settings > Environments, add required reviewers | `android-ai-task.yml` and `apple-ai-task.yml` run only after approval |
| Secret `ANTHROPIC_API_KEY` | Settings > Secrets and variables > Actions | `ai-review.yml`, `*-ai-task.yml` |
| Secret `AEDST_CERT_PINS` | Actions secrets, format `sha256/<live>,sha256/<backup>` | `android-release.yml` fails without pins on purpose |
| Secrets `ANDROID_KEYSTORE_*` | Actions secrets | Without them the release bundle is built unsigned |
| Secret `GITLEAKS_LICENSE` | Actions secrets | Only if the repository belongs to an organization (gitleaks-action v2) |
| Branch protection on `main` | Settings > Branches | Require the aggregate `ci` check of each workflow and CODEOWNERS review |

CLI equivalents (run from a clone with a remote, `gh` authenticated):

```bash
gh api -X PUT repos/{owner}/{repo}/environments/ai-task
gh secret set ANTHROPIC_API_KEY
gh secret set AEDST_CERT_PINS
gh repo edit --template
```

Required reviewers on the environment must be set in the web UI or through `gh api` with a `reviewers` array of user ids.
