# MP3 syscall addresses, strings and buffer boundaries — 2026-11-10

**Integration note:** This report records its original local repair stage. For the current ordinary-directory layout, GitHub publication and portable commands, see [integration details](integration.md).

**Subsequent file-I/O repair:** [recoverable I/O and descriptor ownership/cleanup](file-io-fix.md) supersedes the corresponding open limitations in this historical report. Current verification scripts overlay seventeen maintained source files; original-stage hashes remain preserved evidence.

This local stage changes only code/userprog/exception.cc relative to the memory-loader delivery. The cumulative original-file change set is twelve files. It adapts the implemented MP3 syscall boundary to its paged address spaces, keeping the public syscall numbers, stubs, file-handle scheme and scalar-call behavior. It does not import a different assignment's handler wholesale.

## Implemented boundary

| Existing operation | Maintained behavior |
| --- | --- |
| Create / Open | Copy a NUL-terminated filename into a bounded kernel buffer before backend access; reject empty or invalid names with -1 |
| Read | Validate the entire requested writable destination, read into a kernel buffer, then copy out only the actual bytes returned |
| Write | Validate and copy the complete readable source before invoking backend output |
| MSG | Copy a bounded message completely before output; invalid messages return -1 without output/halt; valid messages retain the historical output-and-whole-machine-halt behavior |
| Close | Keep existing descriptor checks and return convention |
| PrintInt / Add | Preserve their existing scalar services and result/output conventions |
| Exit / Halt | Preserve their nonreturning finish/whole-machine semantics |

Exec, ExecV, Join, Remove, Seek and other declared-but-unimplemented operations are not added by this repair. Unsupported operations and genuine user CPU exceptions retain the existing assertion path. This is validation of the implemented memory-accepting calls, not a new complete syscall implementation.

## Address and copy policy

Nonempty spans require a current user address space and an active nonempty page table of at most NumPhysPages entries. Validation checks the logical limit before adding the size, then checks every covered PTE for validity, nonnegative/in-range physical frame and destination writability. Negative register pointers become large unsigned addresses and fail the logical range check; negative counts and counts above MemorySize fail before allocation. No user pointer is treated as a direct mainMemory offset.

Copies split at each virtual page boundary and use the corresponding physical frame, so nonidentity and noncontiguous mappings work. The whole span is validated before any byte transfer. Successful reads of user memory mark use; actual copy-out bytes mark use and dirty. Failed full-buffer validation does not dirty or partially overwrite an earlier valid page. String probing can mark use on a valid prefix before rejecting a later byte, but produces no backend file effect or partial message output.

The maximum string is 256 bytes including NUL, allowing a 255-byte filename/message payload. A missing terminator within that bound, an invalid later page or invalid physical frame is rejected. An empty filename is rejected before the legacy backend can assert; an empty valid MSG remains permitted. This is a byte limit, not a Unicode-character count or host-path security policy.

Buffer transfers are capped at MemorySize. Scoped heap buffers avoid placing an entire simulated memory image on a kernel thread's stack; a nothrow allocation failure returns -1 before I/O. An empty span may use any pointer, including -1, because it is never dereferenced; the backend still validates its descriptor. No special console-descriptor behavior is added: the existing stub open-file ids remain 1 through 20, despite broader declarations in syscall.h.

Read validates the complete requested range even if EOF could have produced fewer bytes. Otherwise a bad later page could consume file input before a failed copy-out. EOF leaves the untouched destination tail unchanged, and only pages receiving actual bytes are dirtied. Write collects the complete source before any file output. Invalid memory requests return -1 without invoking backend file I/O, creating/truncating a file, consuming a read offset or writing a partial file.

Normal returns update PrevPC from PC, PC from the saved NextPC, and NextPC by four once. Using NextPC preserves a nonsequential branch-delay target instead of always using PC+4. Nonreturning Exit/Halt and valid MSG do not advance through that normal-return path. The public void MSG declaration remains unchanged; invalid MSG's machine return register is -1 and the caller can continue.

## Executed checks

The original handler failed 35 of 39 initial direct cases; four controls passed. These are failing cases, not 35 independent root causes. After repair, all 39 pass. The driver links the real ExceptionHandler/kernel/filesystem objects and loads a real address space with every other physical frame reserved, forcing nonidentity mappings. It sets syscall registers and checks return values and a single PC advance. Invalid-input baseline host open/read/write effects are intercepted and rejected to avoid unsafe baseline OS operations; observed backend attempts still fail the case. Valid calls use real files in separate native temporary directories. A test-only halt wrapper observes valid versus invalid MSG without ending the driver prematurely.

Coverage includes:

- Cross-page Create/Open names, the accepted 255-byte name, empty/negative/at-end/far pointers, missing terminators, an over-limit terminator and an invalid later filename page.
- Missing address-space/table context, zero or oversized page-table metadata and negative/out-of-range frame mappings.
- Exact 300-byte cross-page Read/Write, readable readonly sources, denied readonly destinations, invalid later buffer pages, unchanged destination/file bytes and preserved file offsets on rejection.
- Negative, oversized and extreme counts/addresses; zero-length requests; invalid descriptors; EOF tail bytes and actual dirty-page coverage.
- Saved-NextPC return at a nonsequential target, bounded cross-page MSG, a terminator in the last mapped byte and invalid/unterminated messages that must not halt.

Three separate genuine MIPS runs also pass: one recovery program, two simultaneous identities, and the reversed launch order. The two identities have different filenames and 300-byte payloads. Both complete successfully after negative-pointer, oversized-count, overlong-name and invalid-MSG calls. Their filenames are deliberately placed across a 128-byte virtual boundary; reopening after Write verifies exact round-trip bytes and EOF preservation. A PrintInt near startup causes I/O blocking so the second program loads while the first remains resident. The independent per-identity host files are checked after normal shutdown. This complements the directly forced nonidentity mappings; it is not just an identity-map smoke run.

The full pipeline rebuilds the host without -fpermissive, then reruns all 27 loader cases, eight real memory-pressure batches, 21 scheduling cases, seven lifecycle boundaries, 15 shutdown/CLI cases, six normal scheduling/trace exits and both independent trace reconstructions. All pass. Existing user return values and hw3t1-first ordering are preserved; every integration batch halts exactly once without an assertion.

## Scope and remaining limits

The source of truth remains the current thread's active linear page table. The tested FILESYS_STUB configuration and one-thread-per-address-space model keep that mapping stable across these calls; copy-out revalidates its span. This does not add pinning or transactional semantics for future concurrent unmapping/shared-address-space APIs or real-disk I/O modes.

This repair does not make the legacy host file layer generally recoverable. For example, a bounded but inaccessible host filename may still trigger its pre-existing OpenForWrite assertion, and read/seek/close failure handling and offsets remain backend concerns. Valid paths retain the stub's existing host-filesystem semantics; this is not path confinement. The table is still shared between processes and does not enforce process-specific file ownership. Arithmetic overflow policy for Add and general CPU-exception recovery are also outside this memory-boundary repair.

No machine simulator, AddrSpace, scheduler/lifecycle implementation, syscall header/stubs, original PDF or compiler binary changes in this stage. The original recovery snapshot, ZIP and bundle are unchanged. Earlier MP1/MP2/MP4 sources remain unchanged. GitHub has not been updated in this local stage.

## Reproduce and evidence

From this full recovered working repository on Linux:

```sh
bash scripts/verify-user-memory.sh /path/to/new-results
```

Use a new output directory. Requirements remain C++11 g++, GNU-style linker wrapping, make, ed, Python 3, multilib and the i386 runtime for the bundled legacy MIPS compiler. The script overlays the twelve maintained files in a native temporary export, removes archival binaries from build success, runs all direct suites, builds the course/pressure/recovery user programs and runs their integrations. The new recovery source is compiled twice with different TEST_ID values. No generated program or file is written into tracked source paths.

[Portable verification](user-memory-verification.json) records source hashes, before/after cases and all suite outcomes. Raw logs and the cumulative patch are archived locally under audit-output/mp3-user-memory-verified. Earlier stage reports/JSON retain their historical evidence. The next separate work item is MP3 host file-I/O error handling and its ownership policy.
