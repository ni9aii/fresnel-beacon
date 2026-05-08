# DevOps Review — fresnel-beacon Phase 2

**Scope:** CI/CD pipeline, build system, testing strategy, release process, documentation, repository hygiene.
**Commit reviewed:** `d922c3c` (phase2: integration final polish and system info logging)

---

## Summary

The Phase 2 DevOps posture is solid for a hobby project but has notable gaps in test coverage, CI efficiency, and release safety. The pipeline runs three parallel jobs (unit tests, static analysis, build + simulate) on every push to `main` and on PRs, with path-based triggers. Release workflow gates on test + analyze jobs and publishes artifacts with SHA-256 checksums. However, there is no ESP-IDF-native testing (Unity/CMock), no firmware size regression tracking, no artifact signing, and the Wokwi simulation scenario is minimal. Documentation is good but the README project structure is stale (missing Phase 2 components).

---

## Findings

### [CRITICAL] No ESP-IDF target tests; host-only tests do not exercise component integration

**File:** `test/test_beacon_math.c`, `test/test_led_driver.c`, `.github/workflows/ci.yml` (job `test`)

The unit-test job compiles and runs two host-side C files with `gcc`. These tests validate:
- `beacon_math.h` inline functions (`pixel_index`, `angle_diff`)
- Mocked GRB byte-order and bounds logic for `led_driver`

They do **not** compile against ESP-IDF headers, do **not** link actual component code, and do **not** exercise:
- FreeRTOS queue/mutex semantics (`ipc`, `config_manager`)
- RMT peripheral initialization (`led_driver`)
- NVS read/write paths (`config_manager`)
- WiFi state machine or HTTP server request handlers (`wifi_manager`, `http_server`)
- Animation task timing or IPC command processing (`beacon_animation`)

For an embedded firmware project, host mocks are useful but insufficient. The absence of ESP-IDF Unity/CMock tests (or even `idf.py build` within a test component) means the majority of the firmware is only validated by "it builds" and a 2-step Wokwi serial wait. This is a critical coverage gap for a project entering Phase 2 with 6 components and a REST API.

**Recommendation:** Add a `test/` component using ESP-IDF's Unity test framework (see `examples/system/unit_test`). Run it in CI with QEMU or on-hardware self-test runner. At minimum, add component-level build tests that `#include` each component's public headers and verify they compile against ESP-IDF.

---

### [CRITICAL] Release workflow uses example credentials, producing a firmware binary with dummy WiFi SSID

**File:** `.github/workflows/release.yml`, lines 50–51

```yaml
      - name: Generate credentials.h
        run: cp main/credentials.h.example main/credentials.h
```

The release job copies the example template (containing `your_network_name` / `your_password`) into `credentials.h` before building. The resulting `fresnel-beacon.bin` artifact therefore embeds dummy credentials. Users downloading the release binary will flash a firmware that attempts to connect to a non-existent SSID named `your_network_name`. This is a broken release artifact.

The CI workflow (`ci.yml`) handles this correctly by substituting secrets into the template via Python. The release workflow should do the same, or the project should switch to runtime WiFi provisioning (e.g., via the HTTP server or a captive portal) so the binary is credential-agnostic.

**Recommendation:** In `release.yml`, reuse the Python credential-generation step from `ci.yml` (with `secrets.WIFI_SSID` / `secrets.WIFI_PASS`). Alternatively, refactor `wifi_manager` to read credentials from NVS at runtime and ship a binary that starts in AP mode for first-time configuration.

---

### [WARNING] Wokwi simulation scenario is a smoke test only; no functional validation

**File:** `scenario.yaml`

```yaml
steps:
  - wait-serial: "Fresnel Beacon starting"
  - wait-serial: "init RMT on GPIO 39"
```

The scenario waits for two boot log lines and then exits. It does not validate:
- HTTP server startup (`http_server_init` log)
- WiFi connection success (`WIFI_STATUS_CONNECTED` log)
- Animation frame output (no pixel color validation)
- REST API responsiveness (no HTTP request simulation)

A passing CI simulation therefore only proves the firmware boots far enough to initialize RMT. A regression that breaks WiFi, HTTP server, or animation would still pass CI.

**Recommendation:** Extend `scenario.yaml` with additional `wait-serial` checkpoints for WiFi and HTTP server init. If Wokwi supports UART-based scripted HTTP clients, add a step that verifies `/api/status` returns JSON. If not, document the limitation explicitly in `WOKWI.md`.

---

### [WARNING] CI `build-and-simulate` job does not depend on `test` or `analyze`

**File:** `.github/workflows/ci.yml`

The three jobs (`test`, `analyze`, `build-and-simulate`) run in parallel. A static-analysis failure or a unit-test failure does **not** block the ESP-IDF build or the Wokwi simulation. This wastes CI minutes and can produce misleading green checks on PRs where the build artifact is generated from known-broken code.

**Recommendation:** Add `needs: [test, analyze]` to the `build-and-simulate` job, or at least to the Wokwi simulation step, so that build resources are only consumed after fast checks pass.

---

### [WARNING] No firmware size regression tracking or size budget enforcement

**File:** `.github/workflows/ci.yml`, lines 109–110

The build job runs `ls -lh build/*.bin build/*.elf` as an informational step. There is no comparison against a baseline, no size budget, and no automated comment on PRs. For a 4 MB flash target (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB`), unbounded growth is easy to miss.

**Recommendation:** Add a size-diff step that compares `build/fresnel-beacon.bin` against the last `main` build (cached artifact) and fails if growth exceeds a threshold (e.g., +5 % or +10 kB). Alternatively, use `idf.py size` and parse the output for `.text`, `.data`, `.bss` budgets.

---

### [WARNING] `sdkconfig.defaults` disables watchdog panic in development mode without CI enforcement

**File:** `sdkconfig.defaults`, lines 8–11

```
# CONFIG_ESP_TASK_WDT_PANIC=n is intentional for development:
# a panic reboot would mask the root cause when debugging via serial.
# Switch to =y in production to ensure automatic recovery.
```

The comment acknowledges this is a development setting, but there is no CI check or build variant that verifies the firmware works with `CONFIG_ESP_TASK_WDT_PANIC=y`. A release build could inadvertently ship with panic disabled, leaving devices hung on watchdog triggers rather than rebooting.

**Recommendation:** Add a second `sdkconfig.release.defaults` (or `sdkconfig.production`) that sets `CONFIG_ESP_TASK_WDT_PANIC=y`, and build it in CI (perhaps in the release workflow) to ensure it compiles and passes simulation.

---

### [WARNING] Missing `dependabot.yml` for GitHub Actions pin updates

**File:** `.github/workflows/ci.yml`, `.github/workflows/release.yml`

Workflows pin third-party actions to specific commit SHAs (good practice), but there is no automated mechanism to open PRs when new versions are released. Over time, security patches for `actions/checkout`, `actions/cache`, `actions/upload-artifact`, or `espressif/esp-idf-ci-action` will be missed unless someone manually checks.

**Recommendation:** Add `.github/dependabot.yml` with a `github-actions` ecosystem entryset to weekly checks. Example:

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
```

---

### [SUGGESTION] README project structure diagram is stale (missing Phase 2 components)

**File:** `README.md`, lines 51–77

The tree diagram only lists `led_driver`, `beacon_animation`, `test/`, and `main/`. It omits the six Phase 2 components:
- `components/ipc/`
- `components/config_manager/`
- `components/wifi_manager/`
- `components/http_server/`

Additionally, the "Features" checklist (lines 31–34) marks "Web UI over WiFi for runtime configuration" as unchecked, but Phase 2 implemented it. The architecture diagram (lines 38–47) also omits `wifi_task`, `http_server`, `config_manager`, and `ipc`.

**Recommendation:** Update the README tree, feature checklist, and architecture diagram to reflect Phase 2 reality. Stale documentation erodes trust in the repo.

---

### [SUGGESTION] No `CHANGELOG.md` or release notes automation

**File:** `.github/workflows/release.yml`, lines 71–76

The release workflow hard-codes release notes:

```yaml
--notes "ESP32-S3 firmware for WS2812B 8×8 matrix lighthouse lamp.
Flash with: esptool.py write_flash 0x0 fresnel-beacon.bin"
```

There is no `CHANGELOG.md`, no automated generation from conventional commits, and no link to the commit range or PRs included in the release. This makes it hard for users to know what changed between versions.

**Recommendation:** Adopt a lightweight changelog format (Keep a Changelog) or use `gh release create --generate-notes` to auto-populate release notes from merged PRs and commit history.

---

### [SUGGESTION] CI artifact retention is short (7 days) and not pinned for releases

**File:** `.github/workflows/ci.yml`, lines 113–119

CI firmware artifacts are retained for 7 days. The release workflow uploads artifacts to the GitHub Release, which is correct, but there is no intermediate artifact retention strategy for nightly or branch builds. If a regression is discovered after 7 days, the exact binary that produced a failing CI run may be gone.

**Recommendation:** For the `main` branch, increase retention to 30 days or use a dedicated "nightly" pre-release tag that preserves artifacts indefinitely.

---

### [SUGGESTION] No container or devcontainer for reproducible local builds

The project requires ESP-IDF v5.3.2, which is a large toolchain. New contributors must install it manually or rely on CI for feedback. There is no `.devcontainer/`, `Dockerfile`, or `docker-compose.yml` for one-command local builds.

**Recommendation:** Add a `.devcontainer/devcontainer.json` using the official `espressif/idf` Docker image. This lowers the barrier for contributors and guarantees the same toolchain version as CI.

---

### [NITPICK] `.gitignore` does not ignore `wokwi-serial.log`

**File:** `.gitignore`

The CI simulation uploads `wokwi-serial.log` as an artifact, but if a developer runs `wokwi-cli` locally, the log file is not ignored and could be accidentally committed.

**Recommendation:** Add `wokwi-serial.log` and `*.log` to `.gitignore`.

---

### [NITPICK] `ci.yml` and `release.yml` duplicate test and analyze job definitions

**Files:** `.github/workflows/ci.yml`, `.github/workflows/release.yml`

Both workflows define identical `test` and `analyze` jobs. This violates DRY and means a future fix (e.g., adding a new static-analysis flag) must be applied in two places.

**Recommendation:** Extract `test` and `analyze` into a reusable workflow (`.github/workflows/_checks.yml`) and call it from both `ci.yml` and `release.yml` via `uses:`.

---

### [NITPICK] `cppcheck` suppresses all missing-include errors globally

**File:** `.github/workflows/ci.yml`, line 59–60

```yaml
--suppress=missingIncludeSystem
--suppress=missingInclude
```

Suppressing `missingInclude` hides configuration errors where `cppcheck` cannot find ESP-IDF headers. While understandable for ESP-IDF (which is not installed in the analyze job), it also suppresses legitimate missing local headers. A better approach is to install ESP-IDF in the analyze job or use `cppcheck` with a project file that provides include paths.

**Recommendation:** Either install ESP-IDF in the analyze job (heavy) or generate a `compile_commands.json` from a build and run `cppcheck --project=compile_commands.json` so includes are resolved correctly.

---

## Conclusion

The fresnel-beacon Phase 2 DevOps foundation is functional: CI builds, tests, and simulates on every push; releases are automated with checksums; and documentation is present. However, the project is carrying significant technical debt in three areas:

1. **Test coverage:** Host-only mocks are insufficient for a 6-component embedded firmware. ESP-IDF Unity tests or at least component-level build tests are needed.
2. **Release safety:** The release workflow ships a broken binary (dummy WiFi credentials). This must be fixed before any `v*` tag is pushed.
3. **CI efficiency and hygiene:** Parallel jobs waste resources, artifact retention is short, documentation is stale, and there is no automated dependency updates for Actions.

Priority actions:
- Fix `release.yml` credential generation (critical).
- Add ESP-IDF component build tests or Unity tests (critical).
- Make `build-and-simulate` depend on `test` + `analyze` (warning).
- Update README structure and feature checklist (suggestion).
- Add `dependabot.yml` for Actions (warning).
