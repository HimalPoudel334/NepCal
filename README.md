# nepcal

A tiny native Windows tray app that shows today's Nepali (Bikram Sambat) date.
Plain C, Win32 API, no runtime dependencies, one small exe.

- The tray icon shows the BS day number.
- Hover for the tooltip: `Saturday, असोज 31, 2083 BS`.
- Saturdays are drawn in red. Other days follow the system theme (light text on a dark taskbar, dark text on a light one).
- Right-click the icon for **Exit**.
- Refreshes every minute, so it rolls over at midnight on its own.

## Files

| File | Purpose |
|---|---|
| `main.c` | Tray icon, icon rendering, theme detection, message loop |
| `date.c`, `date.h` | AD <-> BS conversion engine from [aatishnn/nepdate](https://github.com/aatishnn/nepdate) |
| `Makefile` | Builds `nepalical.exe` with `clang-cl` |
| `build.bat` | Loads the VS Build Tools environment, then runs `make` |

## Requirements

- Visual Studio Build Tools (C++ workload + Windows SDK)
- LLVM `clang-cl`
- GNU Make (`winget install ezwinports.make`)
- Git Bash or any shell that can run `build.bat`

## Build

```bash
./build.bat          # build nepalical.exe
./build.bat clean    # remove build artifacts
```

Use `build.bat` rather than calling `make` directly. It runs `vcvarsall.bat` first so `INCLUDE`/`LIB` point at the Windows SDK. Plain `make` from a regular shell fails with `'windows.h' file not found`.

Exit the running tray icon before rebuilding, otherwise the exe is locked.

## Install

1. Copy `nepalical.exe` somewhere permanent, e.g. `C:\Program Files\NepaliCalendarUtitly\` (needs an admin terminal).
2. Run it once.
3. Make the icon always visible: drag it out of the tray overflow (^), or go to Settings -> Personalization -> Taskbar -> Other system tray icons and switch it on.

### Start at login

Run in `cmd.exe` (no admin needed, it writes to `HKCU`):

```
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v NepaliCalendar /t REG_SZ /d "C:\Program Files\NepaliCalendarUtitly\nepalical.exe"
```

From Git Bash, prefix the command with `MSYS_NO_PATHCONV=1`, otherwise `/v`, `/t` and `/d` get mangled into paths.

Verify with `reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v NepaliCalendar`.

After a rebuild, copy the new exe over the installed one. The Run key points at the installed copy, not your build folder.

## Updating the calendar data

BS month lengths are published by Nepal's calendar committee year by year, so no formula can compute them. The table is the `bs[91][13]` array at the top of `date.c`. Each row is `{year, Baisakh ... Chaitra}`, and the table covers BS 2000-2090 (about AD 1944-2033).

**Do not trust the rows for recent and future years blindly.** The original table had a wrong row for 2083 (Ashwin listed as 30 days instead of 31, so the app showed Kartik 1 on Ashwin 31). The current row is:

```c
{2083, 31, 31, 32, 31, 31, 31, 30, 29, 30, 29, 30, 30},
```

Ashwin 31 and Kartik 30 are confirmed. Mangsir 29 and Magh 29 come from a single calendar site. Poush, Falgun and Chaitra are derived so the year totals 365 days (Baisakh 1, 2083 = Apr 14, 2026; Chaitra 30 = Apr 13, 2027). Spot-check them when you get there.

Later rows (2084 onward) look like placeholders; 2083 and 2084 were identical in the original. When a new year's official calendar comes out, compare it with a trusted patro (e.g. Hamro Patro) and fix that row. Each row must sum to 365 or 366 days.

Outside the table's range the tooltip shows "BS date unavailable" and the icon falls back to the AD day number in gray.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `'windows.h' file not found` | Build with `./build.bat`, not bare `make` |
| `make: command not found` | `winget install ezwinports.make`, reopen the terminal |
| Link error mentioning `RegGetValueW` | Make sure `advapi32.lib` is in `LIBS` in the Makefile |
| Date is off by a day from a month boundary | Check that year's row in `date.c` against an official calendar |
| Can't drag the icon to the always-visible area | Use the Settings toggle instead; stale entries from renamed exes can confuse the drag UI |
| Icon missing after a rebuild | Autostart only fires at login. Run `nepalical.exe` again |
| Build fails writing the exe | The old instance is still running. Exit it from the tray first |

Closing the terminal does not kill the app: it is a `/subsystem:windows` build with no console attached.

## License

`date.c` and `date.h` are GPL-3.0 (from aatishnn/nepdate). Anything you distribute that includes them falls under the GPL. This is a personal tool, so it doesn't matter here.
