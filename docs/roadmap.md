# Improvement and work plan

Updated: 2026-11-03. This is a prioritized work plan, not a release schedule or verification report. Check an item only after its acceptance criteria are met. Record test results, environment, artifact hashes and remaining limitations in [verification.md](verification.md). Proposed features below do not authorize installation, publication or changes to an installed host.

## Completed baseline

- [x] Windows x64 plugin using the unmodified TrafficMonitor V1.86 API v7 header.
- [x] ICMP, TCP and HTTP/HTTPS checks with stable group/check IDs and atomic configuration saves.
- [x] Group indicators, failure hysteresis, stale-result handling and independent monitoring/display settings.
- [x] Bounded workers and launch rate, module retention, concurrent reads and active-probe unload tests.
- [x] English, Russian, Simplified Chinese and Traditional Chinese UI selected from the host language, with English fallback.
- [x] Restart notices based on the effective indicator layout exported at load, including pending changes after reopening settings.
- [x] Offline help through the Help button and F1, with localized resources and controller tests.
- [x] Empty localized LOCAL, WEB and SSH defaults on first launch, with an explicit additive action for existing configurations.
- [x] Local OpenSSH Host import with preview/selection, duplicate handling and TCP-check conversion; unsupported routes and syntax are explained rather than executed.

These items describe implemented behavior. They do not imply that every desktop, theme or network scenario has been verified. See the current automated results in [verification.md](verification.md). Earlier Russian settings/SSH preview, both Chinese settings layouts and Simplified Chinese help scrolling were visually inspected in the standalone DLL host. The new monitoring table has automated coverage; its desktop inspection remains open. The full theme/DPI/font matrix, remaining desktop help combinations and native-speaker review remain open. SSH endpoint import is distinct from the proposed NetWatch configuration export/import below.

## P0: verification before a release candidate

### Desktop settings and help

- [ ] Exercise the updated DLL in an isolated official V1.86 x64 host.

Acceptance: inspect Russian and English settings/help, open help by button and F1, scroll/copy its text, close with its button and Escape, and preserve unfinished edits. Confirm check-only Save produces no restart notice, display changes do, reopening retains the notice, and restarting applies the layout. Record whether each observation used a pristine binary or a modified test host. A screenshot alone does not verify Save or keyboard behavior.

### Rendering, themes and scaling

- [ ] Verify GDI and D2D host paths, light/dark themes and supported Windows scaling.

Acceptance: cover 100%, 125%, 150% and 200% scaling, plus movement between monitors with different DPI where available. Labels, circles, controls and help text must remain legible and reachable; dialogs must fit the usable desktop. Inspect real tooltip hover/placement as well as text. If a combination is unsupported, document it rather than claiming coverage from the existing 150% GDI capture.

### Trusted HTTPS success

- [ ] Add a controlled successful HTTPS fixture alongside the existing invalid-certificate tests.

Acceptance: verify 2xx success, measured response time, hostname validation, and continued rejection of invalid certificates and HTTPS-to-HTTP redirects. Prefer an isolated VM/test environment for any temporary trust setup; do not alter the user's main Windows trust store or disable certificate validation. Retain reproducible fixture instructions and evidence.

### Long-duration and lifecycle behavior

- [ ] Run a documented soak test for at least eight hours with an equivalent plugin-disabled baseline.
- [ ] Exercise sleep/wake, network loss/restoration and host exit while probes are active in a disposable environment.

Acceptance: record host/DLL versions, configuration sizes, sampling interval, CPU time, memory, handles, GDI objects, threads and probe counts. Include idle, normal and overloaded configurations. Define resource expectations before running, investigate sustained growth at steady load, and retain raw observations privately. Confirm the four-worker/four-starts-per-second limits, gray stale results, recovery and safe unload. Do not reinterpret total process cost as plugin-only overhead or treat process termination as proof of graceful worker completion.

## P1: next interface improvements

### Per-check status in settings

- [x] Show each saved check's current state, latest typed measurement, last-update time and error without requiring a manual test.

Acceptance: read a copied engine snapshot without network work in the UI or under the engine mutex. Match results by stable UUID and configuration generation. Distinguish monitoring results from unsaved draft values and manual-test results; an edited destination must not appear to have a result produced for that draft. Preserve selection and edits during refresh. Show disabled, unknown and stale states explicitly. Test refresh during configuration changes and dialog closure.

Implemented with a copied per-group snapshot and a table refreshed every 500 ms. Automated coverage includes live success/failure, failure debounce, stale/disabled/unsaved rows, pending edits and invalid selection, generation changes, independent manual results and dialog closure. Desktop/DPI inspection of this updated table remains part of P0 verification.

### Compact and adaptable settings

- [ ] Reduce unused space and adapt the dialog to smaller work areas and different DPI.

Acceptance: group/check editing, result details, Help, Save and Cancel remain reachable without overlapping controls or clipped Russian/English captions. Support keyboard navigation and keep the existing single-window editing flow. Use findings from P0 scaling checks to choose the layout change; avoid a wholesale UI rewrite without a demonstrated need. Recheck pending drafts, manual tests and Save behavior after layout changes.

## P2: proposed backlog

These features require a separate implementation decision after P0/P1; they are not commitments for the first release.

- [ ] State-change journal. Record timestamp, stable group/check ID, previous/new effective state and reason. Define whether events cover checks, groups or both; distinguish startup, stale data and confirmed failures. Bound storage and retention, avoid disk I/O in callbacks or under the engine mutex, and test rollover and configuration-generation changes. Keep destination details local unless the user explicitly exports them.
- [ ] Configuration export/import. Provide explicit local-file actions, validate imports before replacement, preview conflicts and preserve a recoverable copy. Define UUID handling for replacement versus merging so host identities cannot change silently. Cancel or invalid input must leave the active configuration untouched.
- [ ] Contextual field hints. Explain destination format, timing units and disabled TCP-port fields near the relevant controls. Reuse the offline help vocabulary and verify both languages without crowding the editor.

## Release preparation and publication

- [ ] Resolve P0 findings, or explicitly document and review any remaining release limitations.
- [ ] Configure/build Release, run the relevant automated suite, inspect the diff and scan tracked files for secrets and private evidence.
- [ ] Create a fresh ZIP from the verified DLL, licenses, notices and current user documentation; generate its SHA-256 file and verify the packaged DLL hash. Do not reuse an older archive after rebuilding the DLL.
- [ ] Prepare release notes listing supported host/architecture, changes, installation steps and actual verification limits.
- [ ] Obtain explicit publication authorization, then publish the intended commits/tag/release and verify the downloadable artifact.

Building, committing, pushing and publishing a release are separate actions. Keep test hosts under `.work/`, private research/evidence under ignored `.private/`, and installed TrafficMonitor/IPRegionMon untouched.

## Constraints for every implementation

Follow [AGENTS.md](../AGENTS.md) and [architecture.md](architecture.md). Keep host item objects/metadata immutable until unload and require restart for changes to their exported layout. Preserve UUIDs, TLS validation, bounded probe capacity and module ownership. Add tests at the behavior boundary when a change introduces risk; documentation-only work does not require a binary rebuild. Update this plan and the verification record without marking unperformed checks as complete.
