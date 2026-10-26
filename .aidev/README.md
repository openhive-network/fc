# Automated contributions

This directory configures the automated development platform that contributes to this
repository. This note says which branches it uses here and how its work reaches the
project's main line.

## Branches

- `aidev/integration` — the integration branch. Automated work lands here and nowhere else.
- `main` — the project's main line; reached from `aidev/integration` only by merge request (`aidev handoff`, a linear one-commit-per-change copy on `aidev/handoff`, since `main` is fast-forward only). Merge it keeping the source branch: `aidev/integration` is protected so a merge never deletes it.
- `ai/*` — one branch per task, created and deleted by the platform. Do not commit to them.
- `session/*` — a session's accumulated work, landed on the integration branch as a batch.

## How work reaches the main line

Task and session branches never reach `aidev/integration` through merge requests; the platform
lands them after its own verification. `aidev/integration` reaches `main` by this project's
usual process, merge requests included — the platform never merges into any other branch.

Whether this project's CI pipelines skip `ai/*` and `session/*` branches is decided by the
project's CI rules; maintainers record that decision here.

## Suites run before work lands

Declared in `.aidev/project.yaml` under `verification`, all through `.aidev/run-checks.sh`,
which writes `test-results/<slot>/junit.xml` (one case per step and per test binary; the
Boost.Test binaries also keep their own junit as `<name>.boost.xml`). They run offline in
common-ci-configuration's `ci-base-image-ubuntu`, the image of CI's `build:linux` and `test`
jobs, pinned by digest, with the build tree in `.aidev/build/`:

- `quick` — `command` .aidev/run-checks.sh quick build unit
- `full` — `command` .aidev/run-checks.sh full build unit all
- `baseline` — `command` .aidev/run-checks.sh baseline build
- `canary` — `command` .aidev/run-checks.sh canary build unit
- `static` — `command` .aidev/run-checks.sh static build
- `coverage` — `command` .aidev/run-checks.sh coverage build unit
- `system` — `command` .aidev/run-checks.sh system build unit all

Steps: `build` is CI's `build:linux` (cmake Release, `FC_BUILD_TESTS=ON`); `unit` runs the
binaries CI's `test` job runs, with the same arguments; `all` runs `tests/all_tests`, which CI
does not run, without `fc_network/ntp_test` and `fc_network/websocket_test` (they need the
network), nor the cases that already fail on `main` at onboarding (`7951549`, offline in the
pinned image): `fc/real128_test` (wrong arithmetic/to-string results),
`ip_tests/legacy_address_rejects_ipv6` and `ip_tests/legacy_endpoint_rejects_ipv6` (IPv6
input accepted), and `fc_crypto/rand_test` (statistical bound missed). They are listed in
`ALL_KNOWN_FAILING` in `run-checks.sh`; remove each once it passes. fc has no lint or coverage tooling, so `static` is the build alone and `coverage`
runs `unit` uninstrumented. CI's `build:emscripten` (emsdk image) has no slot yet.
