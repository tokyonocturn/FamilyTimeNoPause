# FamilyTimeNoPause

One file. Drop `version.dll` into your Family Time folder, next to
`FamilyTime.exe`, and the game stops pausing when you alt-tab or minimise it.

No launcher, no injector, no installer, no game file is edited. The mod is a
`version.dll` proxy: the player already imports `VERSION.dll`, so the copy in the
game folder is loaded by the game itself.

> **Unofficial and unaffiliated.** Not made or endorsed by the developers or
> publisher of Family Time. It contains no game code or assets. See
> [Legal](#legal) before using it.

## Install

1. Download `version.dll` from the [latest release](../../releases/latest).
2. Copy it into the folder that contains `FamilyTime.exe` (the same folder as
   `UnityPlayer.dll`).
3. Start the game as usual.

That is the whole install. To remove it, delete `version.dll` from that folder.

The folder normally contains no `version.dll`, so nothing is overwritten. If one
is already there, stop: another mod owns that name.

You can also use the scripts:

```powershell
powershell -ExecutionPolicy Bypass -File install.ps1 -GameDir "C:\Games\Family Time"
powershell -ExecutionPolicy Bypass -File uninstall.ps1 -GameDir "C:\Games\Family Time"
```

## What it does

Family Time stops simulating when it stops being the foreground window. Unity
decides that from window messages, and this build ships with `runInBackground`
off. `Application.runInBackground` is exposed, but its setter does not stick in
these builds (it reads back `False`), so the reliable lever is to stop the engine
from ever learning it was deactivated.

The DLL does exactly two things:

1. **Forwards `VERSION.dll`.** `UnityPlayer.dll` imports three functions from
   `VERSION.dll` (`GetFileVersionInfoA`, `GetFileVersionInfoSizeA`,
   `VerQueryValueA`). All 16 standard exports are forwarded to the real system
   DLL at `C:\Windows\System32\version.dll`, so version queries still work.
2. **Subclasses the game window.** On a worker thread it waits for the player's
   top-level window, then swallows `WM_ACTIVATEAPP(FALSE)`,
   `WM_ACTIVATE(WA_INACTIVE)`, `WM_KILLFOCUS` and `WM_NCACTIVATE(FALSE)`. Every
   other message goes to the original window procedure unchanged.

It writes a short log to `%LOCALAPPDATA%\FamilyTimeNoPause\log.txt` so you can
see whether it loaded and what it blocked.

Nothing here reads or writes game memory of another process, touches saves, opens
a network connection, or installs anything outside that log file.

## Verified in game

Measured on **Episode 3, DX11, b108**, on a copy of the installation. A
diagnostic shim that reports the game's own simulation clock was injected in both
runs, with its focus shield disabled, so it could not influence the result. Each
run pressed Alt+Tab until a different process owned the foreground (confirmed as
`Freebuff` in both), then left the game alone for 15 s.

| Run | Simulation clock advance | Mod log |
| --- | --- | --- |
| No mod (control) | **3,410 ms** across 15 s wall | none (not installed) |
| Mod installed | **19,363 ms** across 15 s wall | `swallowed WM_ACTIVATE(WA_INACTIVE) #1`, `swallowed WM_ACTIVATEAPP(deactivate) #2` |

The control run stops advancing shortly after it loses the foreground. With the
mod it keeps running. The game's clock is not a wall clock (it runs faster than
real time here, which is why 19.4 s of simulation fits in 15 s), so the comparison
between the two runs, not the absolute number, is the result.

## Compatibility

| Build | Status |
| --- | --- |
| Episode 1, `FamilyTime_Patreon` | Expected to work (same player, same import) |
| Episode 2, `FamilyTime_Patreon-DX11/DX12-Build37` | Expected to work (same import) |
| Episode 3, `FamilyTime_Ep_3_WINDOWS_DX11_b108` | **Verified** (table above) |
| Episode 3, DX12 | Not tested |

"Expected" means the player imports `VERSION.dll` the same way, so the DLL loads.
Only the Episode 3 DX11 run was measured, so that is the only claim made.

## Antivirus

A DLL that lives in a game folder under a system DLL's name is the same shape as
a DLL-search-order hijack, so some scanners will flag it. This one is open source,
unobfuscated, has no network code, no persistence and no self-modification, and
its only unusual act is subclassing the window of the process that loaded it.

Windows Defender on the machine used to build it reported no threats for the
released DLL. That is one engine's opinion on one file at one signature version,
not a promise. If your scanner objects:

- verify the `SHA256SUMS.txt` from the release,
- read `src/version.c` (it is the whole mod),
- build it yourself with `build.ps1`,
- or report the false positive to your vendor.

There are no instructions here to disable your protection, and there never will
be.

## Build

Needs 64-bit Windows and a C compiler.

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

MinGW-w64 `gcc` is the default. For MSVC:

```cmd
cl /LD /O2 /D_WIN32_WINNT=0x0601 src\version.c /link /OUT:dist\version.dll user32.lib
```

`build.ps1` prints the export list and writes `dist\SHA256SUMS.txt`.

## Known limitations

- 64-bit builds only. The game's players are 64-bit.
- If a future game build stops importing `VERSION.dll`, the DLL simply never
  loads and the game pauses as before. Nothing breaks.
- The game must be restarted after adding or removing the DLL.
- It does not unpause anything else, change time scale, or touch saves.

## Legal

Unofficial and unaffiliated with the developers or publisher of Family Time. You
must own or otherwise legally access the game, and you are responsible for
complying with its terms of use. Using external tools may violate those terms.
No game code, assets, binaries or save files are included or modified. Use at
your own risk.

## Licence

MIT, see [LICENSE](LICENSE).
