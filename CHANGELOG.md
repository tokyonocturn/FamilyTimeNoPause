# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-09

First release.

### Added

- Drop-in `version.dll` proxy for Family Time. The player imports `VERSION.dll`,
  so a copy beside `FamilyTime.exe` is loaded by the game itself: no launcher, no
  injector, no installer and no game file modified.
- All 16 standard `VERSION.dll` exports are forwarded to
  `C:\Windows\System32\version.dll`, so version queries keep working.
- Window subclass that swallows `WM_ACTIVATEAPP(FALSE)`,
  `WM_ACTIVATE(WA_INACTIVE)`, `WM_KILLFOCUS` and `WM_NCACTIVATE(FALSE)`, so the
  player keeps simulating while another window is in front or the game is
  minimised.
- Log at `%LOCALAPPDATA%\FamilyTimeNoPause\log.txt` showing whether the shield
  loaded, which window it attached to, and which deactivation messages it blocked.
- `install.ps1` / `uninstall.ps1` helpers, and a build script that prints the
  export table and the SHA-256 of the built DLL.

### Verified

Measured on Episode 3 DX11 b108 with a diagnostic read-only shim reporting the
game's simulation clock, focus shield disabled, both runs deactivated by Alt+Tab
until another process held the foreground:

| Run | Clock advance over 15 s wall |
| --- | --- |
| Without the mod | 3,410 ms (pauses) |
| With the mod | 19,363 ms (keeps running) |

The mod log for the second run recorded two swallowed deactivation messages.

### Notes

- Subclassing happens on a worker thread and re-checks the window every 2 s, so a
  resolution change that recreates the window does not lose the shield.
- Only Episode 3 DX11 was measured. The other builds use the same player import.
