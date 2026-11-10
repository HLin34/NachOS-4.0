# Known issues and verification limits

These findings describe the original submission. MP2-01 through MP2-07 now have focused local repairs. The file-layer leak/error findings also have local repairs; MP1 user-memory validation is now repaired; MP4-01 and MP4-03 now have local repairs with explicit analyzer limitations; MP4-02 now has a FIFO repair with explicit progress assumptions; starter-header compatibility remains open.

| ID | Finding | Evidence | Required regression |
| --- | --- | --- | --- |
| MP1-01 | Opening when the 20-entry table is full leaks the host file descriptor; also present in MP2 | `filesys/filesys.h`, OpenAFile | Repeated overflow opens do not increase host descriptor use |
| MP1-02 | User addresses and sizes lack validation | `userprog/exception.cc` | Invalid addresses, unterminated names, negative sizes |
| MP2-01 | `frameTable[128] = {0}` writes outside a 128-element array rather than initializing it | `threads/kernel.cc:96` | All frames initially free; no out-of-bounds access |
| MP2-02 | Exact-page-size code reads zero bytes for its final page | `userprog/addrspace.cc:202` | Code segments of one and two full pages |
| MP2-03 | Initialized data uses the same file offset on successive chunks | `userprog/addrspace.cc:228` | Distinct data on successive pages |
| MP2-04 | Read-only data repeatedly translates the same destination address | `userprog/addrspace.cc`, RDATA branch | Distinct read-only data on successive pages |
| MP2-05 | Original: failed Load can leave members uninitialized before destruction; constructor initialization repaired locally | AddrSpace constructor, Load, destructor | Missing executable exits cleanly |
| MP2-06 | Insufficient memory reaches a generic fatal assertion | ExceptionHandler default path | Explicitly defined failure policy and frame cleanup |
| MP2-07 | File syscalls still treat virtual pointers as physical offsets | `userprog/exception.cc` | File I/O under nonidentity and cross-page mappings |
| MP4-01 | currentTurn and cancelled are accessed without synchronization | `table.cpp`, `philosopher.cpp` | Linux race detector and lifecycle checks |
| MP4-02 | Turn checking does not guarantee starvation freedom | `philosopher.cpp:85,102` | Fair scheduling design plus progress argument |
| MP4-03 | Negative remainder and hardcoded five break neighbor generality | `philosopher.cpp:85` | Wraparound and parameter checks |
| MP4-04 | Bonus relies on modified table.hpp despite header replacement described by course | `table.hpp` and report | Compare with recovered starter header |

MP1 Write error behavior also depends on the framework's stub OpenFile wrapper. Historical report screenshots do not prove boundary-case correctness. A prior Windows MP4 compile and one smoke run passed; that does not establish Linux portability, race freedom, or fairness.

For environment-specific results, see [build.md](build.md). Preserve original reports and document corrections separately.

## Linux baseline execution update

After installing multilib, both host simulators and the selected MIPS examples built on Linux-native storage with `-fpermissive -std=gnu++11` for host compilation and corrected toolchain `-B` paths. MP1 fileIO_test1 reported successful file creation; fileIO_test2 printed Passed; both exited normally. MP2 printed 9,8,7,6 and 15–19 (interleaved at the character level), and both return messages, but did not exit before the 20-second timeout (124). The shutdown cause and local repair are now documented in [mp2-shutdown-fix.md](mp2-shutdown-fix.md). These are baseline examples, not exhaustive correctness tests.


## Local lifecycle repair, 2026-11-09

MP2 batch completion is now tracked explicitly; original timeout reproduced, seven repaired regression cases passed. See [repair details](mp2-shutdown-fix.md). The frame-table and segment-loader findings were subsequently repaired; see [memory repair](mp2-memory-fix.md).

## Local memory repair

MP2-01 through MP2-04 are repaired locally: eleven memory cases pass, including fragmented frames and repeated reclaim; the seven shutdown cases pass again. See [scope and evidence](mp2-memory-fix.md). These later received focused local repairs; see the subsequent sections.

## Local syscall memory-copy repair

MP2-07 is repaired locally. Seventeen direct cases and three dual-MIPS-program runs pass; all eleven memory and seven shutdown regressions remain passing. See [scope, policies and evidence](mp2-syscall-copy-fix.md). Insufficient-memory recovery (MP2-06) was subsequently repaired; full-table descriptor cleanup and host I/O error behavior remain open.

## Local insufficient-memory repair

MP2-06 is repaired locally: eight direct capacity/recovery cases and five real MIPS pressure batches pass. All prior 38 regression cases pass again. See [policy and evidence](mp2-memory-limit-fix.md). Failed loads do not abort surviving programs; simulator-wide exit status and automatic retry remain unchanged.

## Local file-resource and I/O repair

MP1-01 and its MP2 counterpart are repaired. Stub host I/O errors return failure/actual transfer counts rather than asserting, and remaining handles are reclaimed at FileSystem destruction. Both assignments pass 20 focused cases, their MIPS recovery programs pass, the MP1 bundled examples pass, and all 51 MP2 regressions pass. See [scope and evidence](file-io-resource-fix.md). MP1-02 was subsequently repaired; per-process file ownership remains open.

## Local MP1 user-memory repair

MP1-02 is repaired at the existing implemented syscall boundary. All 28 direct cases, a real MIPS invalid-input recovery program, and the prior MP1 file examples/recovery pass. See [scope and evidence](mp1-user-memory-fix.md), including limits of the original loader protections and constructor initialization.

## Local MP4 synchronization and stop repair

MP4-01 and MP4-03 are repaired locally. All 24 Linux functional cases and two scoped ThreadSanitizer cases pass. Forced cancellation inside libc sleep retains analyzer diagnostics, independently reproduced with minimal protected-state probes; these runs are not counted as clean. MP4-02 fairness and MP4-04 course starter-header compatibility remain unresolved. See [scope and evidence](mp4-lifecycle-fix.md).

## Local MP4 fairness repair

MP4-02 is now addressed by FIFO two-fork reservations: no later request bypasses an enqueued request. Progress requires eventual scheduling and finite resource holding; there is no wall-clock bound. Thirteen new functional cases, eight scoped fairness TSan cases, 24 lifecycle regressions and two complete-worker scoped TSan cases pass. Table now requires id-aware wait/signal calls. MP4-04 course header compatibility remains open. See [policy, proof, API change and evidence](mp4-fairness-fix.md). Earlier sections describe their historical verification stage.

## Recovered MP3 maintenance

MP3's previously unresolved source is now recovered from the exact gitlink target. Focused repairs cover build blockers, CPU accounting/preemption, batch shutdown, malformed/cross-page loading, memory shortage, syscall memory boundaries and file-error/ownership cleanup. See [MP3 repair records](../NachOS-4.0_MP3/README.md). Unsupported syscalls/user exceptions, paths/symlinks as a host security boundary, the real simulated-disk filesystem and non-Linux teardown behavior remain outside the tested configuration. MP1/MP2 per-process file ownership remains open; MP3 ownership is now enforced.
