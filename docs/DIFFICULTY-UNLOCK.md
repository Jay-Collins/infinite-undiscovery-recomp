# New-game difficulty override

`iu_unlock_difficulties` is a live, default-off player option under F4 → Game.
The existing Save settings action persists it in the content profile's runtime
configuration. It makes Hard and Infinity selectable the next time the native
New Game difficulty list is constructed. A list already open must be reopened.

The title task (`sub_82477158`) calls `sub_82499008` at `0x824785D0` with a
four-byte output at its stack pointer +100. This reads completion bytes from
the Xbox profile. The title then tests byte 2 to include option 12 (Infinity),
or byte 1 to include option 11 (Hard), alongside options 9 and 10. Without these
completion bytes, the list contains only options 9 and 10.

`difficulty_unlock.h` overrides that query only when the setting is enabled,
the return address is `0x824785D4`, and the output is the expected readable
four-byte stack slot. It supplies `{0,1,1,0}` and a successful return value.
Other callers, including profile initialization in `sub_824994B8`, execute the
original function. Turning the option off executes the original query again.

Only the temporary selection-query result changes. The hook does not write
profile completion records, achievements, save data or the running difficulty.
The game handles the selected new-game difficulty through its original path.

Validation: the Release build compiles, and all 17 host tests pass. The hook
test covers default-off behavior, the enabled title call, exact memory-write
bounds, preserved call context, unrelated callers, invalid output contracts,
unreadable memory and restoration of native results when disabled. The settings
integration test checks live application and persistence of both true and false.
The new-game selection has not yet been manually verified in the running game.
