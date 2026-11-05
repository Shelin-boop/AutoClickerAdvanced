# AutoClicker Advanced 0.3.1

Autoclicker for Linux. It has up to seven lines: a click at the mouse
position, clicks at three fixed positions and three pattern reactions. Every
line clicks in its own interval; only one click runs at a time.

AutoClicker Advanced is a modified version of
[XClicker](https://github.com/robiot/xclicker) 1.5.1 by robiot and the
XClicker contributors (GPL-3.0). Thanks to them for the original program.
This is an unofficial version and is not affiliated with or endorsed by the
XClicker project. Up to version 0.3.0 it was called "XClicker Advanced".

## Download and start

1. Download `AutoClicker_Advanced-0.3.1-x86_64.AppImage` from
   [Releases](https://github.com/Shelin-boop/AutoClickerAdvanced/releases).
2. `chmod +x AutoClicker_Advanced-0.3.1-x86_64.AppImage` (once), or set
   "Is executable" in the file properties.
3. Double-click the AppImage or start it in a terminal.
4. From now on AutoClicker Advanced is also in the start menu. If you move the
   AppImage, start it once from the new place and the entry follows.

Requirements: x86_64, glibc 2.34 or newer, GTK 3, a Wayland session with
Xwayland or an X11 session. Developed for Kubuntu 26.04 with KDE Plasma 6.6;
other systems are not tested.

Settings of version 0.3.0 and earlier (`~/.config/xclicker-advanced`) are
taken over on the first start, and the old start menu entry is replaced.

## Use at your own risk

- Many games, especially online games, forbid autoclickers or other
  automation in their terms of use. Using this program there can get your
  account suspended. Check the rules of the game before you use it.
- The program is provided without any warranty (GPL-3.0, sections 15 and 16).

## Use

| Element | Meaning |
|---|---|
| Interval (ms) | How often the line clicks, 50 to 60000000 ms. Safe mode: at least 100 ms. |
| Mouse position | Clicks where your mouse is. Waits while you move the mouse. |
| Position 1 to 3 | Clicks only at the given position. The mouse pointer stays where it is. |
| Get | Shows a banner. Point at the target in the game and press the start/stop hotkey (default F8). Esc cancels. |
| Hold (ms) | How long the button stays pressed. Empty = 50 ms. "Random": between 20 ms and this value. |
| Options | Hotkey mode (Normal: press to start and stop; Hold: runs while the key is held), stop after a number of clicks, random variation of the intervals. |

All lines are due once right after the start. If several lines are due at the
same time, the one that has waited longest clicks first. Coordinates are
screen pixels as Xwayland sees them (on a 4K monitor 0 to 3839 and 0 to 2159).
While the AutoClicker Advanced window is active, coloured rings show positions
1 to 3.

## Pattern reactions

1. Bring the game to the front and press F9 (or click "Screenshot" and switch
   to the game during the countdown). The editor opens with the screenshot.
2. Tool "Pattern": drag a square tightly around the pattern (8 to 200 px).
   The inner part of the circle (80 %) is compared.
3. Tools "Frame 1" to "Frame 3": drag the areas to watch. Frames must not
   overlap. "Exclusion zone": areas inside a frame without reactions.
4. "Test detection" shows every hit on the screenshot with its similarity
   (green = would react). Then "Save".
5. Switch on "Reaction 1" (and 2, 3), set the interval (default 200 ms) and
   start with F8.

A reaction clicks the center of a pattern that newly appears in its frame,
at most one per check. Patterns already visible at the start are ignored,
except at the place where the pattern was taken. A clicked place is locked
for 1 s. The status line shows the current similarity per frame, the
duration of a check and the last reaction.

## Settings

Tabs "General", "Mouse and timing", "Pattern reactions" (threshold, search
factor, compared part of the circle, color tolerance, lock radius and time,
ignore patterns visible at the start, screenshot key and countdown).

Safe mode, start/stop hotkey, mouse movement (pause threshold, rest time,
also pause positions 1 to 3, hold the mouse off the game during a position
click), click timing (wait before pressing and after
releasing, double click gap), start menu entry, reset.

A click at a position is sent as: move, wait, press, hold, release, wait,
move back. With the defaults (40 ms wait, 50 ms hold) it takes about 130 ms;
a mouse position click takes about 50 ms. Between two clicks there is at
least the wait time. If a game still misses clicks, raise the wait.

A game has only one mouse pointer. During a position click (about 130 ms)
AutoClicker Advanced holds your real mouse off the game, so the click stays on
target even while you move the mouse. Your own movements and clicks in that
short time are queued and reach the game right after the click. While you
hold a mouse button, and 500 ms after you release it, all automatic clicks
wait, so your own clicks always work.

## Build

```
make            # binary in build/autoclicker-advanced
make appimage   # AppImage in dist/
```

Needs gcc, pkg-config, glib-compile-resources and the development packages
for GTK 3, X11, Xi, Xext and cairo (gdk-pixbuf comes with GTK 3). `make appimage` also needs appimagetool
and its runtime file (see `pkg/build-appimage.sh`).

## Documentation

Requirements, feasibility test and development notes (German) are in
[`docs/`](docs/README.md). The change list per version is in `NOTICE.md`.

## License

GNU General Public License v3.0, see `LICENSE`. The changes compared to
XClicker 1.5.1 are listed with dates in `NOTICE.md`. The icon
(`assets/icon.svg`) is an original drawing under the same license.

The AppImage file also contains the AppImage runtime with its own licenses
(MIT, LGPL-2.1, BSD and others); see `THIRD-PARTY-NOTICES.md` and the folder
`licenses/`.
