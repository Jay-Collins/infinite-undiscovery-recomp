# Experimental 60 FPS

Implemented for the USA executable used by this project. Default remains 30 FPS.
F4 > Settings > Graphics > Frame rate selects 30 or 60; save and restart.

## Mechanism

- The initialization call returning to `0x821CB6B8` invokes `sub_826CE9C8`
  with `r4=30`. This sets offset 196 of the timing object referenced at
  `0x82A5840C`.
- `src/frame_rate.h` changes only this verified request to 60 when
  `iu_frame_rate="60"`. Other callers and requests are untouched.
- `sub_821B8240` waits for `60 / target` video ticks. It records elapsed ticks
  at offset 132 and computes elapsed seconds at offset 168; offset 192 is
  the time multiplier. The normal game update calls it at `0x821B79AC`.
- The existing scheduler thus supplies 1/60 second instead of 1/30 second
  at full frame rate. No global speed multiplier, refresh-rate change,
  VSync disable, or generated-code edit is required.
- Keep the normal 60 Hz guest video clock and VSync enabled. This does not
  establish compatibility with manually altered SDK timing settings.

## Validation, 2026-11-03

Private instrumentation recorded host elapsed time, update and swap counts,
accumulated simulation time, last delta, time multiplier, target, and tick count.
The initial candidate used an environment toggle at the same initialization
hook; the player option replaces that toggle with a persisted setting.

- Original mode: 29.990 updates/sec over 45.816 seconds; accumulated simulation
  time / host time = 1.00002. Delta 0.03333334, scale 1, two video ticks.
- Candidate: sampled steady interval (182.044092 to 211.326593 seconds) showed
  approximately 59.76 swaps/sec. Delta 0.01666667, scale 1, one video tick;
  simulation time continued at approximately real time.
- Intro playback includes periods around 30 FPS despite the 60 target.
- User confirmed smoother gameplay at normal speed after loading a save.
  User reported hitches mainly for new areas/effects. Shader compilation or
  asset loading is a hypothesis, not a measured attribution of those hitches.
- All 12 existing tests passed, including new frame-rate validation,
  persistence, and restart-notice checks in the menu settings test.

This is not full-game validation. Combat edge cases, long scripted sequences,
and other content still require broader playtesting. Do not advertise hitch-free
60 FPS or universal cutscene compatibility.

## Private recording

Configure `IU_ENABLE_TIMING_EXPERIMENT=ON` to compile the timing recorder.
It writes `frame-timing.csv` in the working directory. This option is OFF by
default and explicitly disabled by `release/make-release.ps1`. It does not
enable 60 FPS; use the player setting. Preserve logs before another launch.

The initial local captures are in `out/smoke/fps60-gameplay/baseline-30.csv`
and `frame-timing.csv`; these private output files are not release assets.
