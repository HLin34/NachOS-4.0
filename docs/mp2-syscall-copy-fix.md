# MP2 user-memory copying for file system calls

## Repaired behavior

The original exception handler treated user virtual addresses as physical offsets into mainMemory. Under multiprogramming, this can select another frame's bytes, corrupt the wrong destination, or pass an invalid host pointer to file operations.

The local MP2 handler now validates and copies through the active page table for Create, Open, Read, Write, and the pointer-bearing diagnostic MSG call. Close and scalar arguments retain their existing interface. Four missing Open/Read/Write/Close assembly stubs were added to MP2's start.S so actual MIPS programs can link and invoke the supported kernel handlers.

## Defined boundary behavior

- Filename and diagnostic strings require a NUL within 256 bytes, so filenames contain at most 255 non-NUL bytes. Copying can cross virtual page boundaries.
- Nonempty buffers must fit wholly within the mapped virtual address space. Every covered entry must be valid and map to a physical frame in range. Read destinations must also be writable.
- Negative or oversized byte counts are rejected. Zero-length operations require a valid file ID but do not dereference the pointer.
- Invalid Read/Write buffers return -1 before file I/O. Invalid Open returns -1; invalid Create preserves the existing boolean failure value 0. Invalid MSG returns -1 rather than halting through an invalid pointer.
- Read copies only the actual returned bytes. Bytes beyond EOF remain unchanged in the destination.
- Copy-in sets use; copy-out sets use and dirty. Read-only source pages can be copied in; read-only destinations are rejected.
- A bounded kernel buffer bridges the file interface and noncontiguous physical frames. The complete requested span is checked before consuming file bytes or modifying user memory.

The helpers use the active machine page table rather than invoking hardware exceptions for bad syscall pointers. This allows the syscall to return an error normally. Existing program-counter advancement remains covered on success and error returns.

## Evidence and validation

The direct test driver links the actual kernel exception handler and stub filesystem. It supplies fragmented, nonidentity page mappings; switches between two independent mappings; and checks user bytes, host file bytes, return values, program counters, use/dirty flags, and unchanged file offsets after rejected requests.

Seventeen cases cover cross-page filenames and data, independent contexts, EOF tails, invalid ranges, a read-only destination, invalid/negative-frame mappings, negative sizes and pointers, zero size, invalid IDs, unterminated strings, unmapped string tails, and invalid diagnostic messages.

The MIPS integration fixture is compiled with the bundled cross compiler. Two programs concurrently create separate files, write 300 bytes across multiple pages, reopen/read/compare those bytes, and exit explicitly with their actual success or error code. Both filenames deliberately cross a page boundary. The runner checks both successful exits and the exact host file contents. It passes under default scheduling and random seeds 7 and 23.

| Suite | Result |
| --- | --- |
| Original syscall handler with current loader/lifecycle repairs and restored user stubs | 14 of 17 direct cases fail; all 3 MIPS integration runs fail |
| Repaired handler and user stubs | 17 direct cases and 3 MIPS integration runs pass |
| Existing memory suite after syscall repair | 11 pass |
| Existing shutdown suite after syscall repair | 7 pass |

Direct-case success requires the test driver's completion marker as well as exit 0, preventing an unexpected simulator Halt from masquerading as a successful test. MIPS fixtures call Exit explicitly because the historical startup code otherwise exits with 0 regardless of main's return value.

Evidence is saved under `../../audit-output/mp2-syscalls-final/`, `mp2-syscalls-handler-before/`, `mp2-syscalls-memory-regression/`, and `mp2-syscalls-shutdown-regression/`. Intermediate build failures are retained separately; they are not counted as runtime evidence.

## Reproduce

Run from the preparation directory in Ubuntu, choosing new output directories:

```sh
bash scripts/test-mp2-syscalls.sh ../syscall-results-new
NACHOS_SYSCALL_BASELINE=1 bash scripts/test-mp2-syscalls.sh ../syscall-before-new
```

The baseline mode retains the original exception handler while using current loader/lifecycle repairs and the same restored stubs. Its nonzero exit is expected. This isolates syscall translation from previously repaired memory and shutdown defects.

Builds and socket-using runs use Linux-native storage. The scripts overlay the explicitly listed six MP2 source files on the preserved historical export. Source PDFs, MP1 and MP4 are unchanged.

## Remaining work

This repair resolves MP2-07. The subsequently completed [memory-limit repair](mp2-memory-limit-fix.md) addresses MP2-06. The existing shared open-file table, full-table descriptor leak, host I/O failure behavior and per-process descriptor ownership remain separate work. The stub filesystem still operates on host files; the test runs use isolated temporary working directories.

Subsequent [file-resource repairs](file-io-resource-fix.md) resolve the shared full-table descriptor leak and stub host-I/O error paths. Current MP2 reproduction scripts overlay ten source files, including four file-layer files, and the earlier regressions still pass. Per-process file ownership remains separate work.
