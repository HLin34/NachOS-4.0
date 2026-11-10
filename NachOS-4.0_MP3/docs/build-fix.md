# MP3 build repair — 2026-11-10

**Subsequent file-I/O repair:** [recoverable I/O and descriptor ownership/cleanup](file-io-fix.md) supersedes the corresponding open limitations in this historical report. Current verification scripts overlay seventeen maintained source files; original-stage hashes remain preserved evidence.

**Subsequent syscall-boundary repair:** [paged address/string/buffer validation](user-memory-fix.md) now addresses the implemented syscall memory boundary described as open in this historical report. Current scripts overlay twelve files; earlier source hashes and results remain the evidence for their original stage.

**Subsequent memory repair:** [cross-page loading and memory-shortage recovery](memory-fix.md) now supersedes the loader/OOM limits described in this historical report. Current scripts overlay eleven files; original-stage result records below remain unchanged.

**Subsequent shutdown repair:** [safe batch completion](shutdown-fix.md) now resolves the timeouts recorded in this historical report. Current scripts overlay ten files; earlier source hashes and observations below remain the evidence for their original stage.

**Historical stage:** the subsequent [scheduling repair](scheduling-fix.md) supersedes the scheduling observations and five-file overlay described below. The current scripts overlay seven files. Earlier timeout/order evidence remains archived; the standalone build verification JSON below describes that earlier stage.

The recovered archive is complete, but its original simulator could not compile. This local maintenance stage makes the simulator rebuild with modern g++ using `-std=gnu++11`, **without -fpermissive**. It does not repair the audited SJF policy or lifecycle defects.

## Changes

| File | Focused change |
| --- | --- |
| code/lib/list.cc | Qualify four inherited IsInList calls in SortedList with this->; no container-ordering change |
| code/threads/alarm.cc | Remove the invalid E message referencing nonexistent oldThread/nextThread; log actual selections in Yield |
| code/threads/scheduler.cc | Keep the removed thread in a valid local for B and return it after logging; construct valid D strings and log before SWITCH to avoid using an expired pointer after resumption; omit I/O text for a finishing thread |
| code/threads/thread.cc | Correct two misspelled execution-time accessors, construct valid C/E strings, place E inside the non-NULL next-thread branch |
| code/build.linux/Makefile.dep | Remove generated GCC 4.4.7/Red Hat dependency paths; regenerate local dependencies during verification |

The counter increments, burst formula, comparator, queue insertion-before-accounting order, unconditional timer-yield request, process counters and frame lifecycle remain unchanged. Trace timestamps/accumulated values are still subject to the audited accounting defects. Moving a debug message does not establish SJF correctness.

## Build verification

The original audit recorded host build failure even with compatibility flags. The maintained version is freshly exported and overlaid in native Linux temporary storage. Its archival nachos executable is moved away before compilation, preventing accidental use of an old prebuilt. Dependencies are generated with a 32-bit compiler configuration, then all objects and the final simulator are rebuilt with g++ -std=gnu++11 and the existing -m32 configuration. No permissive compiler mode is used.

The three MIPS targets are forcibly rebuilt, including start.S, with relocated -B search paths and the bundled COFF-to-NOFF converter. Tool permissions change only in the temporary export. A normal 64-bit compiler alone remains insufficient: multilib and the Linux i386 runtime are required. The legacy tools execute in Linux-native storage.

The build retains legacy warnings, including string-literal to char* conversions and linker warnings from the original context-switch assembly. This is a successful build, not a warning-free modernization or complete toolchain rebuild.

## Runtime boundary

Smoke runs are observations with an eight-second deadline, not passing scheduling regressions:

- hw3t3 returns 3, both with and without z tracing, but the simulator does not stop before the deadline.
- Both hw3t1/hw3t2 launch orders return 2 before 1 and then time out. The required hw3t1-first property therefore remains unmet in these runs.
- The paired z-trace run exercises the corrected A/B/C/D/E messages. Its precise event counts are retained in the verification results; it is still a scheduling/shutdown observation, not a full pass.

The script separately records build-results.json and smoke-observations.json. A successful script exit reflects completed builds; read each observation's returncode/returns/halted/assertion fields before drawing runtime conclusions. The tests do not repair or certify uninitialized kernel counters, frame recovery, file copying, memory pressure, malformed executables or syscall bounds.

## Reproduction and preservation

Run `bash scripts/verify-build.sh NEW_RESULTS_DIRECTORY` from this working copy under Linux. The script requires the recovered historical commit in local Git history. It overlays only the listed five maintained files; future repair stages must update that overlay set if additional source files change.

Original commit, PDFs, tracked executables and all 291 recovered files are preserved in the isolated original snapshot and backup bundle. The earlier 21 maintained MP1/MP2/MP4 source files remain unchanged. No GitHub changes, merge, visibility change or license replacement occurs at this stage.

The next priority is CPU-burst accounting and timer-boundary preemption selection, followed by orderly batch shutdown and the independent loader/lifecycle findings. Previously extracted nine logical counterexamples are not declared fixed by this compilation repair.

[Machine-readable verification](verification.json) records the exact scope, build outcomes, trace coverage and retained runtime failures. Raw logs and the source-only patch are retained locally under audit-output/mp3-build-fix-verified.
