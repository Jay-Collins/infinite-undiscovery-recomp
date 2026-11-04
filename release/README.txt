Infinite Undiscovery Recomp v1.0.0-rc3 (Jay-Collins fork)
======================================================

Download and source:
https://github.com/Jay-Collins/infinite-undiscovery-recomp

Based on doc-haz's Infinite Undiscovery Recomp:
https://github.com/doc-haz/infinite-undiscovery-recomp
Original contributors are credited in the repository README.

Getting started
---------------
1. Extract the ZIP to a writable folder.
2. Run InfiniteUndiscoveryRecomp.exe.
3. Choose Install / Setup and select your legally obtained Disc 1 media.
   Supported inputs: XDVDFS ISO, or a folder with default.xex, ud1.bin,
   and ud2.bin. Add Disc 2 for later progression. DLC is optional.

Requires 64-bit Windows 10/11, a Direct3D 12 capable GPU, and the Microsoft
Visual C++ Redistributable 2015-2022 (x64). No game data or saves are included.

This build fixes the Orb of Patience pedestal interaction in Vesplume Tower.
The release build was verified in-game: Examine appears and the orb opens
the door. Full-game completion and later Disc 2 progression remain unverified
in this fork.

Controls and saves
------------------
Use an Xbox-compatible controller for gameplay.
F4: open / close the unified menu. F7: open its Achievements tab.
F10: return to the Content Profile Manager. F12: quit (with confirmation).

The menu has Settings, Controls, Achievements and Game tabs. The Game tab
also provides profile-manager and quit actions. Save settings keeps changes
for the active profile; starred settings require a restart. Gameplay input
is blocked while the overlay is open. Gameplay pauses at the next frame
boundary and resumes when the menu closes, including when opened with F7
or Back + Start. Background music continues. The footer indicates when paused.
When a changed setting needs a restart, Save settings and restart appears.
It restarts the same content profile after confirmation. This does not save
game progress: save in the game first and wait for saving to finish.
Aspect ratio preserves the original picture or stretches it to the display.
Display resolution presets apply in windowed mode; borderless fullscreen
uses the desktop resolution. Internal resolution provides uniform 1-8x
native rendering scales and requires a restart.
Graphics also offers 30 FPS (original) or 60 FPS (experimental). Save and
restart to apply. Keep VSync enabled. First-use shader/asset loading hitches
may remain; videos retain their own playback rate. Full-game compatibility
at 60 FPS is not yet verified.

Developer diagnostics, trace recorders, Save Anywhere and movement/recovery
tools are omitted. The runtime debug overlay and console shortcuts are
unregistered. Normal runtime error logging remains available.

Saves created during play stay inside the selected profile, for example
USA/saves/. Back up your existing installation before migrating it; this ZIP
contains no profile configuration, saves, game assets, shader cache or logs.

Save backups are stored in <profile>/backups/saves/. Existing save slots are
backed up at launch; changed saves are copied when their save container closes.
The latest 10 distinct versions are kept per slot. F4 > Game > Save backups
shows local timestamps and offers Restore and restart. Restore keeps a recovery
copy of the current slot first (which may temporarily exceed retention), waits
for the old game process to exit, and replaces only the selected slot. An
interrupted restore is rolled back on the next launch. These backups contain
saved progress only; they are not autosaves or save states. One game session
can use a content profile at a time.

Licenses
--------
See LICENSE, THIRD_PARTY_NOTICES.txt and LICENSES/.
The matching v1.0.0-rc2-LGPL-Compliance archive on the release page provides
runtime source and relink materials. It is not needed to play.
This unofficial project is not affiliated with Square Enix, tri-Ace or Microsoft.
