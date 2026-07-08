<div align="center">

# hopy

**Lightweight · cross-platform clipboard manager** — the window pops up right at your text caret, never steals focus, and pastes back exactly where you were.

[简体中文](README.md) · **English**

<a href="https://github.com/Hunlongyu/hopy/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/Hunlongyu/hopy?style=flat-square&color=6b8cff&label=release"></a>
<a href="https://github.com/Hunlongyu/hopy/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/Hunlongyu/hopy/total?style=flat-square&color=6b8cff"></a>
<a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/Hunlongyu/hopy?style=flat-square&color=6b8cff"></a>
<img alt="Platform" src="https://img.shields.io/badge/platform-Windows-6b8cff?style=flat-square">
<img alt="Qt" src="https://img.shields.io/badge/Qt-6-6b8cff?style=flat-square&logo=qt&logoColor=white">
<img alt="C++" src="https://img.shields.io/badge/C%2B%2B-20-6b8cff?style=flat-square&logo=cplusplus&logoColor=white">

<img src="docs/showcase.png" alt="hopy overview: color-value cards / image cards / hover / live preview" width="100%">

</div>

## ✨ Highlights

- **🎯 Caret-following · never steals focus** — The window opens right at the **text caret** in your editor and never grabs focus, so the original caret stays alive and paste-back lands exactly where you left off.
- **🌈 Color-value cards** — Detects `#hex` (3/4/6/8-digit), `rgb()` / `rgba()`, and bare hex, and draws the swatch inline at the start of the row.
- **🔗 Content-aware open** — For URLs / emails / file paths, one key or click opens them in the browser / mail client / file manager.
- **🔒 Sensitive masking** — Content the OS flags as sensitive (e.g. copied from a password manager) is masked in both the list and the preview.
- **👁️ Live preview + side-button scroll** — Hover or press Space to preview; the info bar shows **type · char count · time**, and long content scrolls with the mouse side buttons `M4 / M5`.
- **📦 Static single file** — On Windows it builds into a **single dependency-free exe** with no Qt DLLs — copy and run.

> Plus the essentials you'd expect: full keyboard control · search & filter · pin / favorite · light & dark themes · English & Chinese · automatic update check.

## 📥 Download

Grab the latest `hopy-*-windows-x64.exe` from **[Releases](https://github.com/Hunlongyu/hopy/releases/latest)** and double-click to run — single file, no install required.

## 🌗 Light / Dark themes

<div align="center">
  <img src="docs/showcase-themes.png" alt="hopy light and dark themes side by side" width="100%">
</div>

## ⌨️ Shortcuts

| Key | Action |
| --- | --- |
| `Alt + C` | Summon the window (default, customizable) |
| `Double-click` / `Enter` | Paste the selected item back |
| `Shift + Enter` | Paste as plain text |
| `1 – 5` | Pick and paste the Nth item |
| `↑ / ↓` · `Home / End` | Move · jump to first / last |
| `← / →` · `Tab / Shift + Tab` | Switch content type (filter) |
| `/` | Focus the search box |
| `O` / `Right-click` | Open link / email / file (customizable trigger) |
| `F` · `T` · `Delete` / `D` | Favorite · pin · delete |
| `Space` (hold) · `M4 / M5` | Preview current item · page the preview |
| `Esc` | Exit search / hide the window |

## 🔨 Build (Windows, static single file)

Requires the static Qt 6.11.1 (MSVC 2026 x64) at the prefix referenced by `CMakePresets.json`, plus Ninja and the MSVC 2026 toolchain.

```powershell
cmake --preset win-static-release
cmake --build --preset win-static-release
ctest --preset win-static-release
```

Output: `build/win-static-release/hopy.exe`.

## 🧱 Tech stack

Qt 6 Widgets (custom-drawn cards · QSS themes) · SQLite persistence · a platform layer (global hotkey / low-level input hook / caret probing / foreground paste-back) · CMake + Ninja. Designed to be cross-platform; currently statically linked on Windows / MSVC 2026 x64.

## 📄 License

[MIT](LICENSE) © 2026 Hunlongyu
