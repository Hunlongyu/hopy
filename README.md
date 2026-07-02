# hopy

Cross-platform lightweight clipboard manager, built with Qt 6 (Widgets) + SQLite.
Static single-file build on Windows (MSVC 2026 x64).

## Build (Windows, static single-file)

Requires a static Qt 6.11.1 (MSVC 2026 x64) at the prefix referenced in
`CMakePresets.json`, plus Ninja and the MSVC 2026 toolchain.

```powershell
cmake --preset win-static-release
cmake --build --preset win-static-release
ctest --preset win-static-release
```

Output: `build/win-static-release/hopy.exe` — a single self-contained executable
with no Qt DLL dependencies.
