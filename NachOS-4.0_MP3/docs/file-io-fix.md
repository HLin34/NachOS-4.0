# MP3 file I/O errors and descriptor recovery — 2026-11-10

This local stage repairs recoverable FILESYS_STUB host-file failures and user descriptor ownership. It adds five modified original files to the preceding twelve-file delivery and extends Thread::Finish, for a cumulative seventeen-file source set. The prior syscall address/string/buffer implementation remains byte-for-byte unchanged. Historical ZIP, recovered snapshot and Git bundle remain separate and unchanged; nothing is published to GitHub.

## Failure and transfer policy

| Operation | Behavior |
| --- | --- |
| Create | Preserve the existing 1/0 backend success/failure result; invalid user filenames still return -1 at the syscall boundary. Host open and close errors return failure without an assertion. A failed close may occur after the file has already been created/truncated. |
| Open | Missing files, host descriptor exhaustion and OpenFile allocation failure return -1. Allocation failure closes the already-open host descriptor. A full user table is rejected before opening a host descriptor. |
| Read / Write | Return actual transferred bytes, 0 for EOF/zero progress, or -1 on error. Retry host EINTR for open/seek/read/write, but do not silently loop to complete a short transfer. |
| Position | Advance the logical sequential position only by a positive returned count. Reject negative positions/counts and signed position-plus-size overflow before seeking. ReadAt/WriteAt and Length do not change the logical sequential position. |
| Length | fstat reports the file length without moving its host offset; retry EINTR, reject errors and sizes outside the int interface. |
| Close | Keep 1 on success and -1 on failure; consume both descriptor and table slot even on close error. Never retry Linux close, including EINTR, because Linux has already released the descriptor and its number may be reused. |

New Stub* wrappers serve the FILESYS_STUB OpenFile path. Existing assert-on-error wrappers used by the simulated disk, console and network are unchanged. There is no global relaxation of device assertions. The shared file offset is set before each transfer; these operations do not yield the simulated CPU, so they remain together in this single-host-thread simulator.

A short write can change a prefix of a file, and the caller receives that exact count. No rollback or atomic-file-write guarantee is introduced. A failed read copies no bytes into the user's destination; a short read copies only its returned prefix through the already-validated paged syscall buffers. An error does not turn into a fabricated successful write count or a negative logical offset.

## Ownership and cleanup

The coursework's shared capacity of twenty slots and public IDs 1..20 remain. Each occupied user slot now also records its owning Thread pointer. SysOpen/SysRead/SysWrite/SysClose pass the current thread explicitly, and foreign read/write/close calls return -1 without touching the owner's file or position. This deliberately removes cross-program use of another program's global descriptor. It is not a new per-process twenty-slot descriptor namespace or descriptor inheritance implementation.

A NULL owner is reserved for direct internal/test calls; it is not a wildcard that can access a user's descriptor. Loader opens remain separate scoped OpenFile objects and do not consume user slots. The earlier user-memory test fixture now registers its file under the current user thread; its 39 assertions and scenario list remain unchanged.

Thread::Finish closes all handles owned by the finishing thread after releasing its address space and before ProgramFinished can halt the batch. CloseAllFor is idempotent, leaves other owners intact, and continues after an individual close failure. FileSystem destruction closes every remaining slot for explicit Halt/MSG or other global teardown. Release removes the slot's ownership before closing, and OpenFile consumes the host descriptor before calling close, so destruction cannot accidentally retry a failed close.

There is no new destructor output or claim that a close failure means nothing reached the host filesystem. Exit has no return channel for individual cleanup failures. Explicit Close reports them; bulk teardown continues to release the remaining resources.

## Evidence

35 direct cases link real filesystem, syscall, loader and thread objects. Host open/read/write/lseek/fstat/close and OpenFile allocation are intercepted only to inject controlled failures or short results. Linux errno constants are captured before including the coursework's syscall errno.h: NachOS defines negative error numbers, unlike host errno. The tests check actual temporary-file contents, user destination tails, logical positions, single PC advancement, /proc/self/fd counts, exact close counts and slot reuse.

Coverage includes missing paths, EMFILE, failed create close, interrupted open/seek/read/write/length, read EIO, write ENOSPC, seek failure, short reads/writes, zero-progress writes, negative/overflowing offsets, failed length, allocation rollback, invalid IDs, repeated full-table attempts, foreign syscall handles, owner cleanup, cleanup errors, double close, close EINTR and destructor cleanup. An actual Thread::Finish reaches a test-only completion observer only after all its handles and address space have been released.

Four real MIPS runs compile three dedicated programs: a single program filling all twenty slots; nine such programs in one batch leaving all handles open; and two interleaved owners in both launch orders. The programs explicitly call Exit with checked success/failure codes because the legacy start stub discards main's return value. Both interleaved programs reject foreign handles and retain access to their own files. Exact file contents, expected exit codes, one halt and absence of assertions are required.

The fresh strict gnu++11 32-bit host build and all prior regressions are also required: 39 syscall-boundary cases, three syscall MIPS runs, 27 loader cases, eight pressure runs, 21 scheduler cases, seven lifecycle cases, fifteen shutdown cases, six shutdown-trace checks and eight scheduling/trace records. Legacy compiler/linker warnings remain; -fpermissive is not used. Current evidence is recorded in [file-io-verification.json](file-io-verification.json); raw logs are under audit-output/mp3-file-io-verified in the surrounding local workspace.

## Reproduce

From a full clone on Linux with the dependencies described in the [README](../README.md), use a new output directory:

```sh
bash scripts/verify-file-io.sh /path/to/new-results
```

The script exports the recovered original commit, overlays the seventeen maintained source files into Linux-native temporary storage, rebuilds the simulator and MIPS targets, and executes all suites. Earlier verification entry points also export the complete current maintained source set; their historical result files retain the hashes and claims of their original stage.

## Remaining limits

Only the FILESYS_STUB linear-page-table configuration is tested. This is not recovery for every host device failure, asynchronous signal handler, host thread race, non-Linux close behavior or the separate simulated-disk filesystem. Symlink/path confinement, durability/fsync and a filesystem security sandbox are not added. Unsupported user CPU exceptions/syscalls retain the previous behavior. Legacy compiler binaries, PDFs and user-program binaries are unchanged; existing copyright notices remain, and no blanket license is assigned.
