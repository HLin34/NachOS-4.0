# MP2 batch termination repair — 2026-11-09

## Reproduced behavior

The original two-console-program example prints both return messages but never halts. The regression reproduces this with a five-second deadline, then confirms that the locally repaired version exits normally.

A debugger stop in the original last `Thread::Finish` shows `consoleIO_test2` entering `Thread::Sleep(finishing=true)` and then `Interrupt::Idle`. At this point the ready list contains zero threads, the pending interrupt list contains two entries, its first entry is `TimerInt`, and the periodic timer remains enabled. `Timer::CallBack` schedules another timer event after each software callback. Thus pending interrupts do not naturally drain after the batch has completed. This is a kernel lifecycle problem independent of whether the sample's printed numbers look correct.

Debug evidence: `../../audit-output/mp2-shutdown-debug.log`. The hardware-emulation source is unchanged by this repair.

## Local repair

- Initialize command-line and thread counters deterministically, and track live user programs in Kernel.
- Register a program before Fork can enable interrupts and start the child. Reserve its thread ID before that handoff.
- Keep a launch-in-progress flag so an early child completion cannot halt the simulator before the rest of the batch has been launched.
- In Thread::Finish, with interrupts disabled, release the finishing program's address space, clear its pointer, and notify Kernel. The thread's execution stack still follows the existing deferred-destruction path.
- Halt after the last registered user program has finished and launching is complete. An empty ready list alone is not used as evidence of completion; a program blocked on I/O remains counted.
- Initialize AddrSpace's pageTable and numPages so an executable-open failure can safely use the same cleanup path.

The repair touches `threads/kernel.{cc,h}`, `threads/thread.cc`, and the AddrSpace constructor in `userprog/addrspace.cc`, all under MP2. It does not alter the original course PDFs or the preserved audit snapshot. It intentionally does not repair the separately recorded frame-table initialization, segment-loader, or syscall-addressing defects.

## Validation

Run in Ubuntu from this preparation directory:

```sh
bash scripts/test-mp2-shutdown.sh ../mp2-shutdown-results-new
```

Use a new results directory. The script builds the fixed historical baseline and a second export with the explicitly listed local repair files. Both builds use Linux-native temporary storage, multilib, the recorded host compatibility flags, and relocated MIPS compiler prefixes. The baseline must reproduce completed output followed by a timeout; unexpected setup failures do not count as a successful reproducer.

| Case | Original | Local repair |
| --- | --- | --- |
| Two console programs | Both return messages; timeout | Pass |
| One console program | Not repeated for this repair | Pass |
| Reversed launch order | Not repeated for this repair | Pass |
| Random scheduling, seed 7 | Not repeated for this repair | Pass |
| Random scheduling, seed 23 | Not repeated for this repair | Pass |
| Missing executable | Not repeated for this repair | Error reported, clean shutdown |
| Missing executable followed by valid program | Not repeated for this repair | Error reported, valid program completes, clean shutdown |

Each repaired case requires exit 0, the expected number of successful user-program return messages, exactly one shutdown message, and no assertion. Missing-executable cases also require the expected diagnostic. This checks batch completion and avoids masking an early shutdown as a passing example.

Results: `../../audit-output/mp2-shutdown-regression-1109/{baseline,fixed}/results.json`, with a separate output log per case and source-build logs. The test deliberately leaves the baseline verifier unchanged: `verify-baseline.sh` continues to test the original commit, while this regression compares original and repaired behavior.

## Remaining limits

This is a focused repair for the coursework batch launcher, not a new general process manager. The existing fixed-size thread arrays, nonzero user exit-status policy, memory exhaustion behavior, page-boundary defects, and other audited issues remain separate work. These seven cases do not establish that all MP2 requirements or hidden tests pass.

The later [syscall repair](mp2-syscall-copy-fix.md) updates these reproduction scripts to overlay six current MP2 source files, including exception.cc and test/start.S. Original scope and historical results above describe this repair at the time it was introduced.

Subsequent [file-resource repairs](file-io-resource-fix.md) resolve the shared full-table descriptor leak and stub host-I/O error paths. Current MP2 reproduction scripts overlay ten source files, including four file-layer files, and the earlier regressions still pass. Per-process file ownership remains separate work.
