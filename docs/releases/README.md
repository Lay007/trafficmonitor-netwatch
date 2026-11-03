# Prerelease preparation and publication

## Prepared candidate

- Tag/title: `v0.1.0-beta.1` / `NetWatch v0.1.0-beta.1`.
- DLL/catalog version: `0.1.0`, matching the existing CMake/plugin metadata.
- Platform: Windows 10/11 x64, TrafficMonitor 1.86 x64, API v7.
- Release assets: `NetWatch-v0.1.0-beta.1-windows-x64.zip` and `SHA256SUMS.txt`.
- Release text: [v0.1.0-beta.1.md](v0.1.0-beta.1.md).
- English/Chinese catalog entries and PR title/body: [catalog-submission.md](catalog-submission.md).
- The DLL is not Authenticode-signed. Checksums establish byte integrity, not publisher identity.

The new settings table has automated coverage but incomplete desktop/DPI/theme inspection. This candidate is explicitly a feedback prerelease. A catalog listing is subject to the upstream maintainer accepting that status.

## Build a reviewable local package

Build/test the intended DLL as documented in the root README and record its SHA-256 in [verification.md](../verification.md). Commit its source before packaging. Then use a new output directory:

```powershell
python tools/package_release.py --output .work/prerelease-v0.1.0-beta.1
```

The script refuses an existing output directory, source changes under `src/`, `vendor/` or `CMakeLists.txt`, a mismatched recorded DLL hash, a non-x64 PE binary, or inconsistent source version strings. It uses an explicit file allowlist, verifies every ZIP entry and writes the external ZIP checksum plus an internal file manifest. The manifest records HEAD as the source revision. Package from the final committed preparation revision so its source/docs can be recovered together. DLL bytes must be unchanged from the recorded verification artifact; rebuilding requires checking the new hash and relevant tests again.

Use `manifest.json` and `SHA256SUMS.txt` from the generated output to identify the exact reviewed candidate. Test the extracted DLL with the ABI host and tooltip-budget regression before publication. No TrafficMonitor binary, personal configuration, private notes or test executable belongs in the ZIP.

## Publication order

These are pending external actions, not steps performed by the package builder:

1. Obtain authorization to push the prepared commits and publish this exact candidate. Review the incomplete P0 checks and explicit prerelease limitations.
2. Push the intended source revision and create `v0.1.0-beta.1` at that revision. Create a GitHub release with **prerelease enabled**, using the prepared text and the two release assets. Do not mark this candidate as stable/latest.
3. Download the public ZIP/checksum and compare against the reviewed hashes. Confirm installation instructions, asset links, source tag and language support are visible.
4. With authorization for upstream submission, open a PR against `zhongyang219/TrafficMonitorPlugins:main` using the prepared patch/title/body. It changes only `download/plugin_download.md`, `download/plugin_download_en.md` and `plugins_version.xml`. If upstream has changed, recheck/rebase the patch before submission. Do not submit links that do not yet resolve.
5. Record the release/PR URLs and actual downloadable-artifact verification. Do not claim a catalog listing until the maintainer merges it.

The GitHub connector used during preparation has no release-asset upload operation. Publishing will require a suitable authenticated API/CLI or manual GitHub release creation; none was performed as part of preparation.
