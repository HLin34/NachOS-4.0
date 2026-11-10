# MP2 frame allocation and segment loading repair

Local repair following the batch-termination fix. The preserved course snapshot and GitHub repository remain unchanged.

## Changes

- Size the frame bitmap with NumPhysPages and initialize each entry. The original assignment to frameTable[128] wrote outside the bitmap without clearing its entries.
- Scan free frames once when allocating the page table; zero each allocated frame, initialize the translation flags, and update the free count.
- Use one page-boundary-aware loader for code, initialized data and read-only data. Each chunk is limited by both the remaining segment length and the current virtual page boundary. Translate each chunk separately and advance both virtual and file offsets.
- Compute the address-space extent from the highest nonempty segment end, including BSS and gaps, then add the stack. Summing segment lengths is insufficient when the virtual layout has gaps.
- Apply page-level read-only protection after loading. Pages shared with writable initialized data, BSS or stack remain writable because this page table cannot protect individual bytes.
- Check payload read lengths. A short payload causes Load to fail and release its allocated frames; later destruction does not release them again.
- Preserve legacy converter compatibility: empty segments have no meaningful address/file-offset metadata, and BSS has no meaningful file offset. Those unused fields are ignored.

This step originally left the insufficient-memory handler and cross-page syscall copying for separate repairs; both are now documented in subsequent repair notes. Full malformed-NOFF validation and overlapping-segment policy are also outside this repair.

## Tests against the actual kernel

The C++ test driver links the real kernel objects and calls Kernel::Initialize, AddrSpace::Load, Translate, and the destructor. It replaces the driver main only. There is no second loader implementation in the test.

Initialization starts with the bitmap deliberately set to all used, making the original incomplete initialization fail deterministically. Loader tests separately reserve every even-numbered physical frame and poison all physical memory. Each expected virtual byte is checked against a generated fixture; reserved neighbor frames must remain untouched. Allocation, successful release, and failed-load cleanup are checked across three cycles per case.

Fixtures include full code pages, a partial final page, unaligned code, initialized/read-only data crossing pages, mixed read-only/writable pages, BSS and virtual gaps, unused legacy metadata, and a truncated payload. The tests also check valid/clean page flags, distinct frames, read-only write rejection, zero-filled gaps and stack, and restored bitmap/free counts.

Before the memory repair, the original loader plus the earlier lifecycle repair failed eight of nine initial cases; the code-tail control passed. Results: `../../audit-output/mp2-memory-before-03/cases/results.json`. Two earlier test-harness setup attempts are retained separately and are not counted as evidence of source failures.

The final eleven memory cases all pass: `../../audit-output/mp2-memory-final-verified/cases/results.json`. The seven batch-termination regressions were also rerun successfully after the loader repair: `../../audit-output/mp2-memory-shutdown-verified/fixed/results.json`. Its original baseline still reproduces the completed-output/timeout failure as expected.

## Reproduce in Ubuntu

From the preparation directory, choose new results directories:

```sh
bash scripts/test-mp2-memory.sh ../mp2-memory-results-new
bash scripts/test-mp2-shutdown.sh ../mp2-shutdown-results-new
```

The scripts export the fixed historical commit and overlay the explicitly listed local MP2 repair files before building on Linux-native temporary storage. They use the previously verified multilib and compatibility settings. Result files and build logs are saved to the requested output directory; the original snapshot is not modified.

These focused checks establish the repaired boundary and resource-lifecycle behavior, not a pass of every MP2 requirement or hidden test.

The later [syscall repair](mp2-syscall-copy-fix.md) updates these reproduction scripts to overlay six current MP2 source files, including exception.cc and test/start.S. Original scope and historical results above describe this repair at the time it was introduced.

The later [memory-limit repair](mp2-memory-limit-fix.md) makes capacity rejection nonfatal and verifies resource recovery.

Subsequent [file-resource repairs](file-io-resource-fix.md) resolve the shared full-table descriptor leak and stub host-I/O error paths. Current MP2 reproduction scripts overlay ten source files, including four file-layer files, and the earlier regressions still pass. Per-process file ownership remains separate work.
