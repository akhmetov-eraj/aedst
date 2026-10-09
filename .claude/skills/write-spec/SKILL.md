---
name: write-spec
description: Turn a short task description (usually a GitHub Issue marked ai-task) into a spec for a human to approve BEFORE any scaffolding or implementation. Use for a new feature, a new event, or a new state-changing command in any app.
---

# write-spec

Produce one spec document, post it for approval, then stop. Do not scaffold, write tests or write implementation code until a human has approved the spec.

## Steps

1. Read the app rules for the app named in the task: `apps/<app>/AGENTS.md` (or `README.md` if the app has no `AGENTS.md`), `docs/agent/INDEX.md`, and `docs/content/library-first/<platform>.md`. Do not propose anything listed in `docs/content/decisions.md` or the platform decisions page as rejected.
2. Find the closest existing module to copy (the `tasks` module exists on every platform) and name it in the spec.
3. Write the spec using the template below. Server specs live at `apps/server/specs/<module>/<feature>.md`; other apps use `apps/<app>/specs/<feature>.md`. Status starts as `Draft`.
4. Post the spec as a comment on the Issue (or print it in the session), ask for approval, and stop.
5. Only after approval: set `Status: Approved`, then proceed with tests first, then implementation, then `task check` and the app's own check task.

## Template (mirrors `apps/server/specs/_template.md`)

```markdown
# Spec: <module>/<feature>

Type: Command | Query
Module: <module>
Status: Draft | Approved | Implemented

## Context
Why this is needed, what problem it solves.

## Scenarios
**Happy path** - Given / When / Then
**Edge cases** - Given / When / Then

## Errors
| Condition | `sharederrors.Kind` | `connect.Code` |
| --------- | ------------------- | -------------- |

## API
rpc <Method>(<Method>Request) returns (<Method>Response);
| Field | Type | Required | Description |

## Events
| Event type | Version | partition_key | Payload | Consumers |

## Cross-module dependencies
A synchronous call to another module (through its api/), if any, and why it is not an event.

## NFR
- Idempotency: is an `Idempotency-Key` required?
- Timeout:
- Hot path: yes/no
```

For a client-only change (no new RPC), drop the API, Events and Errors sections and add: screens touched, strings added (all five locales: en, ru, tg, uz, ar), offline behaviour, and tests planned.

## Rules

- Versions in the spec are verified against a registry or marked `UNVERIFIED`.
- A one-line bug fix does not need a spec.
- Never edit generated files; the spec names the source (proto, sql, tokens) that changes.
