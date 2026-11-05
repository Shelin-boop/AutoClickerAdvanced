# Documentation

The design documents were written in German during development. They record
the requirements, the feasibility test on the target system and the state of
each version. They describe versions up to 0.3.0 and therefore use the
earlier program name "XClicker Advanced". The name of the game used for the
tests was removed and replaced with "[Name des Spiels]"; otherwise the
documents are unchanged.

| File | Content |
|---|---|
| [de/lastenheft-v1.1.md](de/lastenheft-v1.1.md) | Requirements specification (Lastenheft) v1.1, 2026-11-03: goals, target environment, Wayland risk and solution paths, requirements F-01 to F-60, parameters and defaults, phase plan |
| [de/phase0-befunde.md](de/phase0-befunde.md) | Phase 0 findings: feasibility test on Kubuntu 26.04 / KDE Plasma 6.6 (Wayland, Xwayland) and the architecture decision (XSendEvent, XShm capture, X11 overlays, XInput2 hotkeys) |
| [de/phase1-stand.md](de/phase1-stand.md) | Phase 1 state, versions 0.1.0 to 0.2.2: click sequence, timing for games, per-line intervals, pointer grab during position clicks |
| [de/phase2-stand.md](de/phase2-stand.md) | Phase 2 state, version 0.3.0: pattern reactions, matching method, settings, test results |

Markers used in the documents: [verifiziert] = checked against source code or
a source; [Annahme] = assumption, not checked; [Vorschlag] = proposal accepted
with the approval.

The change list per version in English is in [../NOTICE.md](../NOTICE.md),
including the renaming in version 0.3.1.
