# Orb of Patience investigation (2026-11-02)

## Current finding

**Fixed and verified in-game for the supplied slot 3 reproduction.** The user
confirmed that the Examine prompt appears and using the orb opens the door.
The trace independently confirms native activation, v222=40, and s581=1.

Before the correction, the post-handoff save retained one Orb of Patience but
could not interact with the pedestal. The pedestal existed, had its correct
callback, and passed its facing predicate; its line-of-sight predicate failed.
The blocking collider was the pedestal's own box inside a CMapAreaObject.
The fix corrects this endpoint contact without bypassing save state.

## Reproduction and evidence

- Supplied USA `assets/default.xex` SHA-256:
  `9523b45e6a724d4988ce9cf70d55b672e461b89303b02dc2f0f9c84329c25555`.
- Slot 2 is the original imported reproduction save. Slot 3 was saved after
  Aya completed the handoff and the orb-acquired message appeared.
- Slot 3 backup: `assets/orb-repro/post-handoff-20261102-191535`.
  Data SHA-256: `DE28E566C6B929DF5446EF6B5993AB84FB28EDF75FED6BD77826AE21A49C9344`.
- Reloading slot 3 and trying Examine with the sword sheathed still fails.
  The user confirmed the inventory label **Orb of Patience**.
- First focused capture: `out/orb-captures/interaction-line-of-sight-rejected.log`.
  New session starts at host uptime 38915828, after older appended captures.
  Configuration at 38917937: object `E5E3DDB0`, handle `0000039B`, instance 10749.
  Suppression getter returns 0. Callback becomes `00009847` (=38983).
  Position `(-7800,4000,7200)`, box center `(-7800,4050,7200)`, half extents `(50,50,50)`.
  Facing returns 1; sight returns 0, including while Capell is about 61 units away.
  Runtime state remains `v222=30`, `s581=0`.

## Script and save mapping

Scripts were extracted read-only from the supplied discs using the independently
documented SNC/NORM formats in
[vs-sr-dev/pc-infiniteundiscovery](https://github.com/vs-sr-dev/pc-infiniteundiscovery),
checked out under `tools/iu-format-research` at
`3fed8bb0daf795bd0260444685538ecfaf203cce`.
Local helpers: `tools/orb_script_probe.py`, `tools/orb_asm.py`, `tools/orb_rtti_probe.py`.
No extracted game code or assets should be committed.

Vesplume is map 4444. Extracted scene:
`out/orb-captures/scenes/ud2_510DD000_039_SCE.bin`.
Disassembly: `out/orb-captures/vesplume-script.txt`.
The following script addresses are **decimal word offsets**, not PPC addresses.

| Script word | Meaning established from script and native code |
| --- | --- |
| 38889 | Opcode FC creates interaction box h923, instance 10749, callback 38983 |
| 38917 | Handoff path checks Orb of Order (item 849), sets v222=30 |
| 39098 | Handoff scene; removes item 849 at 39187 and adds item 850 at 39192 |
| 38983 | Pedestal callback checks item 849, otherwise item 850 |
| 39035 | Orb-use handler sets v222=40, removes item 850, sets s581=1 |
| 44349 | Updates door h873 using s581 |

The serialized main-state offset is the BE32 at save+8; it changes between
these saves (0x9300 vs 0x9100), so raw file-offset comparisons are misleading.
The script-state block starts at main+564. Native setter `82498818` writes
`v[index]` at state+576+index*8 (type word, then value word).
`v222` changes from 20 to 30 between the saves. Flag s581 is bit 5 of the
BE32 at state+72 and is clear in both saves.
Inventory serialization starts at main+14908; item 850 changes from absent to
quantity 1. This matches the user's inventory confirmation.
The separate 102400-byte persistence array starts at main+23936; changes there
must not be mistaken for the v222 quest variable.

## Native path

| PPC function | Observed or statically established role |
| --- | --- |
| 825AC860 | Opcode FC handler; checks saved suppression flag, creates/configures box |
| 82498588 | Suppression getter for instance 10749 from state+7744 bit array |
| 8263D908 | Sets box transform/half extents; object+632 is authored instance ID |
| 824DFFD0 | Facing predicate, called with player position; returns 1 in capture |
| 8258F798 | Visibility predicate; calls 82520478 and negates hit result |
| 82520478 | Ray query against map and dynamic objects; mask 0x100 for this query |
| 82278498 | Filtered map collision query |
| 826DFBE0 | Dynamic-object ray traversal; excludes target box itself |
| 8242ECE8 | Dynamic collision model test called from 826DFBE0 |
| 825B93F8 | Interaction dispatch, already instrumented by SAVEPOINT diagnostics |
| 8263DBB0 | CTriggerEventBox activation; enqueues object+2804 callback |
| 82498818 | Script v-variable writer |
| 824984F0 | Script s-flag writer |

RTTI gives CTriggerEventBox primary vtable `8204EF34`. Callback at object+2804
is 38983 after initialization; zero during the earlier geometry configuration
call is expected. The box is not suppressed by its saved instance flag.

## Focused diagnostics

Set `IU_TRACE=VESPLUME,SAVEPOINT` and `IU_ORB_INTERACTION_TRACE=1` before launch.
`src/orb_interaction_diag.h` uses original-function wrappers plus a read-only
poll at the existing guest fiber boundary. The poll generation-checks the
tracked box and player handle, reports quest state, and never calls guest code
or writes guest memory. Predicates are sampled (first 16, then every 256);
state/position are recorded at most every two seconds.

Collision tracing records ray endpoints, map hit points/triangles, and dynamic
collider identities. The existing SAVEPOINT dispatcher has one extra observation
call; its original diagnostic implementation is retained.
Movement tracing can be disabled with `IU_ORB_MOVE_TRACE` unset to reduce noise.
All output remains local in `USA/logs/vesplume_trace.log`.

Eight trace/visibility regression tests pass, including physical-address translation,
recorder gating, original-call counts and return/context preservation for the
new wrappers, and the endpoint correction's guards and temporary-vector restoration.
These host tests do not execute game collision math; the local replay below does.
The local checkout also passed a ninth, separate SDK save-header regression test.
That SDK save-import change is separate from this orb fix.

## Collision cause and verified correction

`out/orb-captures/collision-endpoint-contact.log` records the decisive geometry:

- Ray from approximately `(-7738.7583,4159.52051,7205.27539)` to `(-7800,4100,7200)`.
- Blocking object: CMapAreaObject, primary vtable `82006D14`, handle `EB`.
  Its actor-style position fields are not valid; early OBJECT_POS records for
  this class are irrelevant and have been removed from the diagnostic.
- Local box center `(-4800,775,4600)`, half extents `(25,50,25)`.
- Transform is identity rotation plus translation `(-3000,3275,2600)`.
- World box center `(-7800,4050,7200)`, half extents `(25,50,25)`.
- The ray endpoint lies exactly on the box's top face at y=4100. The segment
  distance routine returns zero and the native closed-segment predicate reports a hit.

Thus this is a self-occluding interaction endpoint. The recorded transform and
intersection agree with the geometry; there is no evidence here of a generic
ReXGlue vector-math defect. Original hardware behavior has not been compared.

`src/orb_visibility_fix.h` moves each endpoint coordinate one representable float
step toward the ray origin for this specific query. It requires the verified
visibility caller, mask 0x100, CTriggerEventBox vtable, instance 10749, callback
38983, and exact endpoint. It additionally requires the caller's private stack
vector at SP+80. The original query runs once, with the same registers and
collision routines. An RAII guard restores the endpoint bytes before returning.
The correction is independent of trace settings. It does not write inventory,
quest variables, actor positions, or persistent collision geometry.

Offline verification executes the original generated math functions using the
captured coordinates. `tools/orb_prepare_replay.py` collects 19 functions into
ignored `out/orb-captures/collision-functions.cpp`. `tools/orb_collision_replay.cpp`
and `tools/orb-replay/CMakeLists.txt` build the local replay. It reads the locally
decoded PE's constants and does not access a running process. Results are saved
in `out/orb-captures/collision-replay-results.txt`:

- Five endpoint-contact approaches: original hit=1, corrected hit=0.
- A real obstruction before the endpoint remains hit=1.
- A ray entering the pedestal side before the endpoint remains hit=1.
- A box below the ray remains hit=0.
- Corrected endpoint-only squared distance is `2.384185791015625e-7`.

This is a compatibility correction for the reproduced pedestal interaction,
not a claim to resolve every previously reported Aya/cutscene softlock.

## In-game verification

Capture: `out/orb-captures/orb-fix-verified.log`, final appended session.
The corrected query reports `ORB_SIGHT_ENDPOINT_EXCLUDED ... HIT=0` and the
visibility predicate reports `RESULT=1`. At uptime 40067859 the native dispatcher
activates pedestal instance 10749, callback 38983. At 40067921 the guest script
writes v222=40; at 40068453 it writes s581=1. The user answered **Yes** when asked
whether Examine now appears and using the orb opens the door. This validates
the native progression path, not just a newly visible prompt.

The original slot 2 and post-handoff slot 3 backup remain available locally for
regression. Game assets, decoded game code, saves, captures, and session-local
research/replay helpers are not part of this source change.
