# Changelog

All notable changes to hopy are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.7] - 2026-07-21

### Fixed
- **Favorites**: favorite and pinned clipboard records are now always included in
  the visible history. The configured history limit applies only to ordinary
  records, so older favorites no longer disappear from the Favorites view after
  newer clipboard entries are captured.

## [0.4.6] - 2026-07-21

### Fixed
- **Update**: an installed update now normalises the executable to a stable, version-less
  `hopy.exe` in its folder, instead of keeping the old versioned filename. The name no
  longer goes stale after updating, and the swap restarts from the new path. The previous
  binary is kept briefly as a backup and swept on the next launch. (Takes effect for
  updates made from 0.4.6 onward; an existing file can be renamed to `hopy.exe` once by
  hand — in-place updates then preserve it.)
- **Preview**: long file paths, URLs, and ordinary text now allocate enough height for
  wrapped display lines instead of clipping the final lines.
- **Scrolling**: Ctrl + mouse-wheel preview scrolling now advances by normal text-line
  steps instead of jumping by a page or directly across the progress percentage.

## [0.4.5] - 2026-07-15

### Added
- **Tray**: the hover tooltip now shows the running version on a third line.
- **Preview**: `Ctrl` + mouse wheel scrolls the open preview (in addition to the
  existing side-button momentum scrolling).

### Changed
- **Preview**: text and file previews now use a virtualised text view, so a large
  paste (e.g. a 1000+ line file) shows its full content and scrolls all the way to
  the end — the previous 8000-character cap is gone.

### Fixed
- **Tray**: the **Settings** item now opens the Settings page instead of the
  clipboard list.
- **Tray**: the **Pause monitoring** and **Launch at startup** items now show their
  on/off state at a glance (an accent-coloured icon plus a trailing check mark), so
  it is obvious whether each is enabled.
- **Colours**: only short entries are detected as colour swatches — a long paste that
  merely contains colour values (such as a code file) no longer renders as a colour.

### Internal
- **CI**: fixed a stale `test_settings` assertion (the default theme is `auto` /
  follow-system, not `dark`) that was failing `ctest` and blocking every tagged
  release.

## [0.4.1] - 2026-07-08

### Fixed
- **Autostart**: self-heal the OS "run at login" entry on launch — when enabled it
  is re-asserted so it always points at the current executable (surviving updates
  and moved folders), and a stray entry is cleared when disabled. The path is now
  quoted so a location with spaces still launches.

### Internal
- **CI**: `build-windows` now triggers only on `v*` tags, so a release commit no
  longer double-builds (once for the branch push, once for the tag).
- **CI**: releases take their notes from the matching `CHANGELOG.md` section
  instead of GitHub's auto-generated notes.

## [0.4.0] - 2026-07-08

### Added
- **Privacy**: mask clipboard content that the OS flags as sensitive (e.g. copied
  from a password manager), so it never shows in the panel or previews.
- **Hotkey**: suppress the global activation hotkey while a fullscreen app is in
  the foreground, to avoid interrupting games and presentations.
- **Tray**: richer context menu with pause-monitoring and check-for-updates.
- **Preview**: momentum scrolling with the mouse side buttons and a live scroll
  percentage indicator.
- **Caret**: WPF `WpfCaret` element probe and a line-level UIA range fallback, so
  the panel finds the caret in more editors (WPF apps, end-of-document/empty lines).
- **About**: redesigned card with a version pill, tagline, an aligned meta grid
  (author link, tech, license, copyright) and a link to the project repository.

### Changed
- **Settings** regrouped into General / Activation / Panel & Preview /
  Paste & Privacy / Storage / About (previously Appearance / Behavior / Storage).
- Renamed **"Window position" → "Fallback position"**: the caret is now always the
  primary anchor, and this setting only chooses where the panel appears when no
  caret can be found — the **mouse position** or the **screen centre**.
- Broadened caret detection (single UIA cache request for the focused element and
  its text patterns; a `WM_IME_COMPOSITION` refresh nudge before reading the caret).
- The GitHub button in About now opens the repository home instead of the releases
  page.

### Fixed
- **Privacy**: stop masking ordinary RDP / cloud-clipboard pastes as secrets.

### Internal
- `release.yml` made manual-only so it no longer clashes with `build-windows.yml`
  (tagged `v*` releases are built and published by the GitHub-hosted workflow).

## [0.3.0] - 2026-07-06

### Added
- Themed single-window update flow with a persistent update badge.
- Startup update check throttled to once per day, with a friendly message (and
  reset time) when GitHub's unauthenticated rate limit is hit.
- Embedded Win32 `VERSIONINFO` resource so the exe reports its version.

### Fixed
- The Alt activation hotkey no longer opens the target app's menu bar.

### Internal
- Self-hosted release workflow that builds, checksums, and publishes on a `v*` tag.

[0.4.6]: https://github.com/Hunlongyu/hopy/releases/tag/v0.4.6
[0.4.5]: https://github.com/Hunlongyu/hopy/releases/tag/v0.4.5
[0.4.1]: https://github.com/Hunlongyu/hopy/releases/tag/v0.4.1
[0.4.0]: https://github.com/Hunlongyu/hopy/releases/tag/v0.4.0
[0.3.0]: https://github.com/Hunlongyu/hopy/releases/tag/v0.3.0
