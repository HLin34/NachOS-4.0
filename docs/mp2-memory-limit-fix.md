# MP2 insufficient-memory handling and safe cleanup

## Policy

A user program that cannot obtain its required physical frames fails to load. It reports `MemoryLimitException` through the existing exception handler, closes its executable, and returns FALSE to ForkExecute. The normal ThreadRoot/Thread::Finish lifecycle then removes only that failed program. Other programs remain runnable, and the simulator halts normally after the registered batch finishes.

The handler logs the memory-limit error and returns to the loader rather than asserting or terminating the simulator. It does not pretend a failed program executed successfully. This remains the course framework's batch interface: diagnostics identify failed loads, while the simulator's final host exit status is still 0 after normal batch shutdown. There is no automatic wait-for-memory queue or retry; a later explicit load can use released frames.

## Changes

- Reject an image whose virtual extent exceeds physical capacity, or whose image plus stack needs more frames than currently free, without allocating frames.
- Close the executable before reporting the memory-limit exception. The handler is not responsible for resources owned by Load.
- Count only initialized/committed page mappings during allocation. If the frame scan unexpectedly runs out after a partial allocation, release those mappings, reset the address space to empty, and report failure.
- Preserve cleanup ownership: Load clears its own partial state; Thread::Finish handles process accounting and any remaining address space. This avoids double release and premature termination of surviving programs.
- Read the syscall-code register only for actual syscall exceptions. A loader memory-limit report does not advance a user program's PC or depend on an uninitialized syscall register.

The MemoryLimitException enum remains at its course-required position before NumExceptionTypes. No machine-emulation files, MP1 files or course PDFs were changed for this repair.

## Validation

The six initial loader pressure cases all failed before the repair with the generic exception/assertion path. Evidence: `../../audit-output/mp2-oom-before/cases/results.json`.

The final direct suite links the actual kernel objects and tests:

| Case | Checks |
| --- | --- |
| Oversized including stack | Request needs 129 frames; failure leaves memory, bitmap, free count, file-descriptor count and PC unchanged |
| Remaining capacity | A 65-frame program cannot consume a fragmented 64-frame remainder |
| Zero available | No allocation or corruption when all frames are reserved |
| Exact fit and retry | A 128-frame image loads; another fails; after release, the same failed AddrSpace object loads successfully |
| Two residents and reuse | Two 60-frame residents leave eight frames; a nine-frame request fails, then succeeds after one resident is released |
| Repeated rejection | Eighty rejected loads do not leak executable descriptors or frames |
| Image exceeds physical memory | Rejects an image whose extent itself exceeds the physical memory limit |
| Partial-allocation rollback guard | Fault injection makes the free counter overstate the bitmap; committed mappings and executable ownership are rolled back safely |

The rollback fault-injection case deliberately starts with inconsistent metadata. It checks that the failed request introduces no additional accounting change, does not alter reserved frames, and does not leak a descriptor; it does not claim to repair preexisting metadata corruption.

Five real MIPS batch cases also pass: oversized alone, oversized before a survivor, a survivor before oversized under random scheduling, a full-memory program before a small program, and the reverse order. Each verifies one clear failure diagnostic, the expected surviving completion count, no assertion, and one normal shutdown.

Results: `../../audit-output/mp2-oom-final/{cases,batch}/results.json`, with per-case output and build logs.

Existing regression suites after this repair: eleven memory-loader cases, seventeen syscall cases, three dual-MIPS file-I/O runs, and seven shutdown cases. Evidence directories: `mp2-oom-memory-regression`, `mp2-oom-syscall-regression`, and `mp2-oom-shutdown-regression` under the local audit-output directory.

## Reproduce

From the preparation directory in Ubuntu, choose a new results directory:

```sh
bash scripts/test-mp2-oom.sh ../mp2-oom-results-new
```

The script exports the original historical commit, overlays the six explicitly listed current MP2 repair files, and builds on Linux-native storage. It runs both direct cleanup checks and real MIPS batches. The preserved original snapshot and GitHub repository remain unchanged.

This resolves the targeted simulated-physical-memory shortage path. It is not a general malformed-executable validator, a host-memory allocator recovery mechanism, or a redesign of process exit-status reporting. The open-file-table leak and broader host I/O error behavior remain separate work.

Subsequent [file-resource repairs](file-io-resource-fix.md) resolve the shared full-table descriptor leak and stub host-I/O error paths. Current MP2 reproduction scripts overlay ten source files, including four file-layer files, and the earlier regressions still pass. Per-process file ownership remains separate work.
