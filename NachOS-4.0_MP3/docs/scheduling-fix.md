# MP3 CPU burst accounting and timer preemption — 2026-11-10

**Subsequent file-I/O repair:** [recoverable I/O and descriptor ownership/cleanup](file-io-fix.md) supersedes the corresponding open limitations in this historical report. Current verification scripts overlay seventeen maintained source files; original-stage hashes remain preserved evidence.

**Subsequent syscall-boundary repair:** [paged address/string/buffer validation](user-memory-fix.md) now addresses the implemented syscall memory boundary described as open in this historical report. Current scripts overlay twelve files; earlier source hashes and results remain the evidence for their original stage.

**Subsequent memory repair:** [cross-page loading and memory-shortage recovery](memory-fix.md) now supersedes the loader/OOM limits described in this historical report. Current scripts overlay eleven files; original-stage result records below remain unchanged.

**Subsequent shutdown repair:** [safe batch completion](shutdown-fix.md) now resolves the timeouts recorded in this historical report. Current scripts overlay ten files; earlier source hashes and observations below remain the evidence for their original stage.

This maintenance stage implements the scheduling requirements identified in the restored coursework audit. It builds on the separate compilation repair, changes five scheduler/thread/alarm source or header files, and keeps the machine simulator and user programs unchanged. The cumulative tracked-file repair set is seven files, including the earlier list-template and dependency-file repairs.

## Behavior

Each thread retains a committed CPU-burst total and the totalTicks start of its current RUNNING slice. Leaving RUNNING commits that slice exactly once. A READY interval does not accumulate CPU time. Dispatch starts a new slice, so wall time used by other threads or spent idle is excluded. Using totalTicks includes simulated user and kernel execution in a running slice.

Running-to-ready retains the cumulative burst and does not update its estimate. Running-to-blocked first commits the final slice, then updates the double estimate as 0.5 * completedBurst + 0.5 * previousEstimate and resets the committed burst, even when the burst is zero. Finishing settles the last slice but does not train a new waiting-burst estimate. The D message uses a captured completed burst before reset and is emitted at the blocking dispatch, before switching stacks.

Ready-list keys are max(0, estimate - committedBurst). A running comparison additionally includes the live slice without mutating queue keys. Yield commits its CPU time before reinserting the current thread, preventing an already-sorted key from changing in place. SortedList keeps arrival order for equal ready keys.

Alarm requests deferred preemption only when the ready head is strictly shorter than the live current remaining burst. Equal, zero, longer or absent candidates do not preempt. A wakeup only enqueues a thread; it never directly switches the CPU. The alarm sets a scheduler request flag and calls the existing hardware return-yield hook. Yield consumes the flag and rechecks the decision after pending callbacks before dispatching. This flag also distinguishes a timer-driven preemption from an ordinary voluntary Yield, whose handoff behavior remains available. E messages describe actual timer-preemption dispatches only.

No new direct Interrupt::Schedule calls or changes under code/machine are introduced. The first two Thread data members and context-switch layout remain unchanged; the added Thread operations are methods, not new fields. The class's existing estimate/accessor interface remains available. Scheduler gains inspection and request-consumption methods plus its private pending flag.

## Verification

A clean Linux-native export is rebuilt with g++ -std=gnu++11, the original -m32 configuration and no -fpermissive. The archival executable is moved out of the build path first. All three supplied hw3t1/hw3t2/hw3t3 MIPS targets are forcibly rebuilt with the bundled cross compiler and converter.

**21 focused tests pass.** They link the actual maintained Thread, Scheduler, Alarm and container objects. A minimal kernel-services fixture sets deterministic clocks; it avoids the separately audited ExecAll counters. Test-only linker interposition observes SWITCH and the return-yield request instead of switching real CPU stacks or requiring a real interrupt-handler frame. These tests are not a full simulated-machine run and do not validate general object teardown.

Coverage includes initial and zero bursts, first and final slices, user+kernel ticks, idempotent settlement, READY-time exclusion, repeated preemption/resumption, clamped zero/FIFO ties, live remaining time, fractional values, shorter/longer/equal/empty/idle timer cases, stale-request recheck, wakeup deferral, voluntary yield and insertion after accounting.

The separate **real MIPS integration** runs use the unmodified hardware timer and actual context switching:

- Both launch orders finish hw3t1 before hw3t2, with and without z tracing (four ordering checks).
- The previous compilation-only version finished hw3t2 first in both launch orders; retained earlier logs establish that before/after difference.
- The reverse-launch traced run exercises actual E timer-preemption events; no requirement forces E events when preemption is unnecessary.
- An independent trace checker reconstructs running intervals, verifies the exponential estimate updates, confirms minimum queued remaining keys, checks D/E accumulated-burst values and rejects preemptions without a strictly shorter candidate. It reconstructs both trace orders rather than duplicating the production accounting helper. Printed floating-point values are checked with tolerance for default stream precision; focused tests inspect exact in-memory doubles.

The normal-order traced run has five estimate updates and zero preemptions; the reverse traced run has five estimate updates and three genuine preemptions. The focused timer tests additionally verify that the callback only requests a switch and does not dispatch immediately.

## Retained runtime failures

**Automatic simulator shutdown remains unfixed.** The six bounded integration observations reach their expected user return values, but all hit the four-second deadline afterward; they are not successful whole-process termination tests. Results record scheduling_passed separately from shutdown_completed. The scheduling checker accepts a future normal halt as well as the currently observed timeout, so it does not encode the shutdown defect as a required success condition.

Kernel command counters/bounds, address-space ownership/frame recovery, loader bounds, memory exhaustion and syscall memory validation remain separate audited work. This stage does not claim those defects are repaired or that shortest-job scheduling prevents starvation under an unbounded stream of new jobs.

## Reproduce

From this full recovered working repository under Linux:

```sh
bash scripts/verify-scheduling.sh /path/to/new-results
```

Requirements are g++, make, ed, Python 3, multilib and the Linux i386 runtime for the bundled legacy tools. The script exports the recovered baseline and overlays the seven maintained files, builds on native temporary storage, runs the 21 focused cases, rebuilds the MIPS examples, records six bounded observations and runs the independent trace checker. A fresh result directory is required. Host build warnings from legacy code remain; this is not a warning-free framework modernization.

The simpler verify-build.sh also overlays the current seven files, but only records smoke observations and does not replace the scheduling regression. Its former five-file results are retained as the historical build-repair stage in [build-fix.md](build-fix.md).

[Machine-readable verification](scheduling-verification.json) records current source hashes, case counts and the unresolved shutdown observations. Raw evidence and the cumulative source patch are kept locally under audit-output/mp3-scheduling-verified, outside this repository.

The original recovered 291-file snapshot, ZIP and verified bundle are unchanged. The preceding MP1/MP2/MP4 maintenance sources are also unchanged. No GitHub publication or merge occurs in this local scheduling stage.
