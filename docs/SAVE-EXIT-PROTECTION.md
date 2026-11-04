# Save activity and session exits

The app tracks save-container lifetimes rather than only individual writes.
The title imports `XamContentCreateEx`; the wrapper registers saved-game content
before entering the SDK. Synchronous failures release the entry. For deferred
opens, the host overlay polls the pinned SDK's overlapped result until completion.
Successful opens remain protected through `XamContentClose` and automatic backup
completion. Loading is protected too: `OPEN_EXISTING` does not prove read-only
access. DLC mounts do not create persistent save entries.

The pinned SDK initializes deferred results to `X_ERROR_IO_PENDING` before
returning. The ninth `XamContentCreateEx` argument is at guest `r1+0x54`; r10
contains the 64-bit content size. On a terminal result, the actual mount map is
checked: an existing mount stays protected even if the game has reused the
overlapped memory for a later operation. Polling then stops. Failed replacement
opens also reconcile an older mount which the SDK may already have removed.

Thumbnail changes and guest file writes/flushes receive transient protection.
These SDK operations complete their work synchronously, even when reporting an
overlapped/pending result. Mount and close wrappers serialize their bookkeeping;
the SDK's global lock is held only for its package-map lookup, never backup I/O.

One mutex arbitrates save starts against an exit reservation. UI disabling is
advisory; restart, profile switching and quit handlers check again immediately
before committing the action. A successful reservation holds new save calls
outside the SDK until process exit. If relaunch fails, cancelling the reservation
wakes those calls without altering their arguments or results. Native close
requests use the SDK's `OnWindowCloseRequested` veto point. App-initiated closes
reserve first, since `Window::RequestClose` bypasses that veto.

Save activity temporarily inhibits F4's cooperative pause. It wakes a parked
frame if necessary and allows the title's multi-frame save/close state machine
to finish. The menu stays open and continues blocking gameplay input. Its pause
request takes effect again at a subsequent frame boundary after activity ends.
Blocked exits are not queued; the player must retry.

`save_activity_exit_protection` tests pending and failed opens, overlapping slots,
case-insensitive roots, stale closes, failed replacements, backup completion,
exit/write races, cancelled exits and pause integration. The existing pause,
restart, settings and backup regression tests also run. In-game verification of
the actual save UI and these import hooks remains pending. Forced termination,
crashes and power loss are outside the normal exit protection.
