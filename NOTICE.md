# Notice

AutoClicker Advanced (called XClicker Advanced up to version 0.3.0) is a
modified version of **XClicker** 1.5.1 by robiot and the XClicker
contributors (https://github.com/robiot/xclicker), licensed under the
GNU General Public License v3.0. AutoClicker Advanced is distributed under
the same license (see `LICENSE`). It is not an official XClicker release and
is not affiliated with or endorsed by the XClicker project.

## Changes compared to XClicker 1.5.1

2026-11-05, version 0.3.1:

- Renamed to AutoClicker Advanced (application ID
  `io.github.shelin_boop.AutoClickerAdvanced`, program file
  `autoclicker-advanced`). Settings in `~/.config/xclicker-advanced` are
  moved to `~/.config/autoclicker-advanced` on the first start; the old
  start menu entry is removed.
- New, original icon (`assets/icon.svg`); the icon based on the XClicker
  icon is no longer used.
- The settings dialog shows the license, the missing warranty and the origin.
- Third-party notices and license texts for the AppImage runtime
  (`THIRD-PARTY-NOTICES.md`, `licenses/`).

2026-11-03, version 0.3.0:

- Pattern reactions 1 to 3: each checks its frame in its own interval and
  clicks the center of a newly appeared circular pattern.
- Screenshot (hotkey F9 or button with countdown) and editor for the pattern,
  three frames and up to eight exclusion zones per frame, with a detection
  test on the screenshot.
- Matching: normalized cross-correlation of the inner part of the circle,
  coarse search on a reduced image, final check at full resolution,
  brightness-independent color check.
- Patterns visible at the start are ignored (except at the place where the
  pattern was taken), wobbling known patterns are followed, a clicked place
  is locked for a while.
- Frames and exclusion zones are shown on the screen while the XClicker
  Advanced window is active.

2026-11-03, version 0.2.2:

- The hold-off during a position click no longer discards the user's own
  mouse events: they are queued and reach the game right after the click.
- All automatic clicks wait while the user holds a mouse button and 500 ms
  after the release (adjustable, can be switched off).

2026-11-03, version 0.2.1:

- During a position click the real mouse is held off the game (pointer grab
  on the game window, about 130 ms), so clicks stay on target while the user
  moves the mouse. Can be switched off in the settings.

2026-11-03, version 0.2.0:

- Every line has its own interval in ms (50 to 60,000,000; safe mode at least
  100). The common step duration was removed. Lines that are due at the same
  time are clicked one after another.
- "Repeat" became "Stop after N clicks" (all lines together), as in XClicker.
- At least the event wait time between two clicks; default wait 40 ms.

2026-11-03, version 0.1.1:

- Click timing for games: move, wait, press, hold, release, wait, move back
  (default wait 30 ms, default hold 50 ms), so a game that reads its input
  once per frame sees the release at the target and not at the mouse.
- Option: positions 1 to 3 also pause while the mouse moves (default on).

2026-11-03, version 0.1.0:

- Step sequence instead of a single click loop: mouse position, positions 1
  to 3 (reactions 1 to 3 are prepared for a later version). Each step can be
  switched on and off and has its own mouse button, click type and hold time.
- Clicks are sent with XSendEvent directly to the window under the target
  position. The mouse pointer is not moved, and there is no additional click
  at the mouse position.
- The mouse position step pauses while the mouse moves.
- Step duration minimum: 100 ms with safe mode, 50 ms without.
- "Get" shows a banner and a ring on the screen; the start/stop hotkey
  confirms the position.
- Position markers are shown while the XClicker Advanced window is active.
- Main window and settings dialog are built in code; the XEvent/XTest switch
  was removed.
- The AppImage adds itself to the start menu (can be switched off in the
  settings).
- Configuration file: `~/.config/xclicker-advanced/xclicker-advanced.conf`
  (the original XClicker configuration is not touched).

## Third-party components

The program links dynamically against libraries of the operating system
(GTK 3, GLib, cairo, Xlib, libXi, libXext). They are not part of the AppImage.
The AppImage runtime and its licenses are described in
`THIRD-PARTY-NOTICES.md`.
