# F4 Save Anywhere

Save Anywhere is available under **F4 → Game**. It closes the overlay and opens
the native save-slot screen. The player still chooses a slot and confirms the
save. It uses the normal save format, backup hooks and save-in-progress protection.

Use it during normal exploration, with weapons sheathed and the party out of
combat. The menu always displays this guidance, including when the button is
enabled. Cutscenes, scripted events, transitions, native menus, dialogue and
active save operations prevent use. Unreadable or incomplete state fails closed.

## Implementation

`save_anywhere_state.h` inspects guest state on the game frame thread. The UI
reads a synchronized host snapshot and can queue one request only when that
snapshot permits saving. The button also requires the F4 frame pause to have
completed. After closing F4, the frame thread checks the state again before
creating a native window. A failed check consumes the request; it never opens
the save screen later when combat or an event ends.

The checks are specific to this title's supported executable:

- Global context `0x82A58EA8`, registry `0x82A380F8`. Registry objects are checked
  against handle generation and type, following `sub_82147140`.
- Scripted-event mode is bit 1 of context +112, set by `sub_824EC2A0` and cleared
  by `sub_824ED090`. Additional title mode bits conservatively block saving;
  these include native camp and transition paths. Not every bit has a complete
  semantic name, so the implementation does not treat this mask as a complete
  enum of game modes.
- The native focus stack at registry +48 / manager +212 must be empty. The save
  manager's ready byte +64 and enable byte +72 must be one. The native UI blocker
  at `[0x82A95998]+60`, also checked by the save window, must be clear.
- The current player (context +120), party (+124) and world (handle 201) must
  resolve. The controlled actor must be alive and belong to that party.
- Each living party member's stats pointer at actor +3260 is checked for the
  battle bit at stats +8, mask `0x4000`. `sub_8215DD08` reads this bit;
  `sub_824B7D08` sets it alongside message 1101 from `CPartyBattleTask`.
  This intentionally also blocks weapons-drawn exploration.
- Both target collection counts at party +76 / offsets +132 and +144 must be
  zero. The first collection is consumed by the party combat AI, including
  `sub_8264FE78`; both are cleared together in `sub_825176F0`. Checking these
  conservatively blocks saving with targets retained while weapons are sheathed.

Dispatch follows the native save-point sequence in `sub_8263CBB8`, including
save mode at window +12592, activation, focus and modal state. The old developer
implementation used `0x82A58EB8` for modal/manager calls; this is corrected to
`0x82A58EA8`. A separate guest stack frame keeps the output holder above the
80-byte PPC linkage/argument area, and guest registers are preserved.

The native dialog is monitored until its handle disappears. Modal/input cleanup
waits for save I/O to finish and only restores the owning save manager. There is
no timed unlock or manual forced cleanup of a live window. Production dispatch
runs at the existing frame/pause boundary, not on arbitrary fiber switches.
Developer traces, recovery controls and the old F6 shortcut remain excluded
from public builds.

## Validation

- Release configuration builds with diagnostics and timing experiments disabled;
  staged SDK hashes match the pinned local source build.
- All 16 host tests pass. `save_anywhere_safety` exercises checked memory reads,
  party battle/target checks, mode locks, missing/stale entities and queued request
  handling. It also runs the production dispatcher against a mapped guest-memory
  fixture, substituting only native calls, to check argument layout, corrected
  modal address, preserved registers, duplicate prevention, state changes after
  clicking, save-I/O exclusion and cleanup after dismissal.
- On 2026-11-06 the user loaded their save and confirmed the F4 feature worked
  during safe gameplay. The always-visible usage disclaimer was added afterward.
- Live combat and cutscene rejection across different areas has not yet been
  separately confirmed. The automated fixtures validate the predicates and
  dispatch behavior, not complete coverage of every scripted game event.

The older `SA-01`, `SA-02` and `SA-03` documents describe the original developer
prototype and its research history; this document supersedes their runtime
entry and availability descriptions for the F4 feature.
