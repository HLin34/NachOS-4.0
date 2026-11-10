# MP1/MP2 stub-file ownership and recoverable I/O errors

## Changes in both coursework snapshots

- OpenAFile locates a free table entry before acquiring a host descriptor. The unchanged 20-entry limit now rejects overflow without opening another host file.
- FileSystem destruction deletes remaining table entries, releasing their descriptors. Executable-loading OpenFile objects remain independently owned by their loader.
- Open uses nonthrowing object allocation and closes the acquired descriptor if that allocation fails.
- OpenFile consumes descriptor ownership once. Explicit CloseFile reports 1 on success or -1 on failure and clears the entry; subsequent destruction cannot close that consumed descriptor again.
- File creation uses a recoverable host wrapper instead of the infrastructure wrapper that asserts. Missing-parent and similar host errors return FALSE/0.
- ReadAt/WriteAt use recoverable positioned host I/O. Read and Write advance the object's sequential offset only by a positive actual byte count. EOF returns 0, short transfers return their actual count, and failures return -1 without moving that offset.
- Read/write interruption before progress is retried. Other host errors, including /dev/full, are returned normally. Close is not retried after EINTR, following Linux descriptor-release behavior; retrying could close a reused descriptor.
- Negative counts/positions, nonempty NULL buffers and spans exceeding the legacy int offset range are rejected. Empty buffers remain valid for valid descriptors. Length uses fstat and returns -1 on failure without altering the sequential position.

Host calls remain confined to lib/sysdep.cc. New Try* wrappers serve the stub file classes; legacy assert-based helpers used by console, network and other simulator infrastructure remain unchanged. Existing copyright notices are retained.

## Validation

Each assignment's real FileSystem, OpenFile and sysdep code runs twenty focused cases. GNU linker wrapping injects short reads/writes, EINTR, EIO and completed closes that report an error; these hooks exist only in the test executable. /dev/full and a read-only descriptor exercise actual host error paths.

The suite checks 200 full-table rejections without descriptor growth, slot reuse, destruction of unclosed handles, missing files, failed Create, invalid IDs, double close, invalid sizes/buffers, close-error ownership, preserved offsets after errors, partial transfers, interruption retries, EOF, length failure and positioned access.

Before the repair, each assignment failed fifteen of twenty cases; five controls passed. Afterward both pass all twenty. Evidence: `../../audit-output/file-io-before/{mp1,mp2}/results.json` and `../../audit-output/file-io-after/{mp1,mp2}/results.json`.

Two actual MIPS recovery programs also pass, one in each assignment. They survive failed Create/Open, fill the entire file table, reject repeated overflow, close/reuse entries, recover from a /dev/full write failure, reject negative sizes, then successfully write/read/compare a regular file. MP1 follows its course convention of Halt after a checked success marker; MP2 exits through its repaired batch lifecycle. The runner verifies exact host file bytes, not just process exit status.

The original MP1 fileIO_test1/fileIO_test2 pair still passes. Integration evidence: `../../audit-output/file-io-integration-verified/{mp1,mp2}/results.json`.

All fifty-one existing MP2 checks pass after this file-layer repair: eleven memory-loader cases, eight memory-limit cleanup cases, five pressure batches, seventeen syscall cases, three dual-MIPS file-I/O runs and seven shutdown cases. Evidence directories under audit-output: `file-io-memory-regression`, `file-io-oom-regression`, `file-io-syscall-regression`, `file-io-shutdown-regression`.

## Reproduce

From the preparation directory in Ubuntu, use new results directories:

```sh
bash scripts/test-file-io.sh ../file-tests-new
bash scripts/test-file-io-integration.sh ../file-integration-new
```

Builds use Linux multilib and the previously documented compatibility options. Native temporary storage isolates socket/filesystem operations and legacy toolchain execution. MP2 regression scripts now overlay ten explicitly listed current MP2 source files; MP1's integration build overlays its four file-layer files. The historical baseline verifier continues to export the unchanged original commit.

## Scope and remaining limits

Create may have created or truncated a file before a later close error is reported; this change does not promise transactional rollback of filesystem side effects. The existing int-sized file-offset interface remains bounded. Linux close/error semantics are the verified target, not a portability certification for every historical host.

The shared open-file table remains shared. Global shutdown now releases its remaining entries, but this is not per-process descriptor ownership, nor automatic close of one program's descriptors at its Exit while other programs remain alive.

The later [MP1 user-memory repair](mp1-user-memory-fix.md) adds address/string validation at the exception-handler boundary. These lower-level NULL/count checks do not establish that every user address is safe. MP2's previously repaired paged copy-in/copy-out remains separately tested. Full malformed-NOFF handling and infrastructure I/O assertions are also outside this file-layer repair.

All changes are local. The original audit snapshot, course reports and GitHub repository remain unchanged.

The integration script now overlays five MP1 source files so it verifies the later user-memory repair as well as the original four file-layer changes.
