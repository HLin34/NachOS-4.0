# MP3 cross-page loading and simulated memory shortage — 2026-11-10

**Integration note:** This report records its original local repair stage. For the current ordinary-directory layout, GitHub publication and portable commands, see [integration details](integration.md).

**Subsequent file-I/O repair:** [recoverable I/O and descriptor ownership/cleanup](file-io-fix.md) supersedes the corresponding open limitations in this historical report. Current verification scripts overlay seventeen maintained source files; original-stage hashes remain preserved evidence.

**Subsequent syscall-boundary repair:** [paged address/string/buffer validation](user-memory-fix.md) now addresses the implemented syscall memory boundary described as open in this historical report. Current scripts overlay twelve files; earlier source hashes and results remain the evidence for their original stage.

This stage replaces the restored MP3 loader's unsafe first/middle/last-page handling with bounded per-page copies and makes simulated physical-memory shortage a recoverable load failure. Production changes since the shutdown stage are confined to userprog/addrspace.cc and addrspace.h. The cumulative tracked-file repair set is eleven files. Kernel registration, scheduler policy, system calls, hardware simulation and archived binaries are unchanged.

## Loading and ownership policy

A load opens its executable under scoped ownership, requires a complete NOFF header and validates the magic. An opposite-order header is normalized with an explicit 32-bit byte swap; instruction/data payload bytes are not rewritten. This supports the tested header byte orders, not arbitrary instruction architectures. The local C++ standard headers are included before the legacy framework's min/max macros; no global utility-macro change is needed.

Nonempty segments require nonnegative virtual ranges and stored-segment file offsets beyond the header. Stored payloads must fit the observed file length, and nonempty virtual segments may not overlap. Address arithmetic uses a wide unsigned type before narrowing. The required address space is based on the highest segment end plus the user stack, not the sum of segment lengths, so sparse virtual gaps are represented correctly. Zero-filled gaps, BSS, stack and page padding are preserved.

Before reservation, the frame bitmap and free counter are checked together with interrupts disabled. An image larger than physical memory, insufficient remaining capacity, zero free frames or inconsistent accounting returns failure without allocating other programs' frames. Inconsistent accounting is rejected, not silently repaired. Reservation checks the scan bound, marks ownership before enabling interrupts and zeros each acquired physical page. This is eager allocation; there is no demand paging, eviction, swapping or automatic retry.

For each stored segment, LoadData copies min(remainingBytes, bytesToPageEnd), translating each new virtual page independently. Both virtual and file offsets advance by exactly the transferred chunk. An exact page boundary causes no extra read, a zero segment causes no translation, and an unaligned short segment cannot overwrite following bytes. Every read must return the requested count.

Fresh successful mappings have clear use/dirty flags. Readonly-data pages are protected only when they do not share the page with code, initialized data, BSS or stack. Shared pages remain writable because a PTE cannot enforce subpage protection. Code retains the earlier writable policy; this does not introduce a new W^X policy. Protection is applied after all segment copies, so loader writes do not become user writes or trip the final readonly bits.

Translate rejects absent/invalid mappings, negative or out-of-range physical frames and disallowed writes before marking use/dirty. Allocation, rollback and destruction share a single owned-frame release path. It detaches a machine page-table pointer only if it belongs to this address space, invalidates its entries, returns its frames and resets numPages. Failed candidates cannot detach a resident's active mapping. Destruction after a failed load cannot free the same frames twice. A failed empty object can be retried; loading an already populated object is rejected without discarding its existing image.

## Failure behavior

Header/segment validation errors, short reads and simulated memory shortage report a load error and return false instead of calling RaiseException from the loader. The existing ForkExecute/Thread::Finish path ends that registered program, releases resources and allows other registered programs to continue. No fault is delivered through an unrelated user's PC, and batch completion still waits for all registered programs.

A read that becomes short after allocation rolls back the entire candidate reservation and closes the executable. Free pages may have been zeroed or partially filled before rollback; bytes belonging to resident/reserved frames remain unchanged. This is an ownership/content guarantee for other programs, not an obligation to restore the old contents of unowned free pages.

The existing host exit-status policy is preserved: a completed batch can exit 0 even when a child failed to load, with the error printed separately. Callers must inspect load diagnostics/expected returns. This is not new exit-status propagation or a policy that retries a child after another program releases memory.

## Executed evidence

| Suite | Result |
| --- | --- |
| Initial real-loader baseline | 27 cases: one basic control passed, 26 failed |
| Maintained real-loader tests | All 27 pass |
| Real MIPS memory-pressure batches | All eight pass |
| Existing scheduling methods | All 21 pass |
| Existing lifecycle boundaries | All seven pass |
| Existing shutdown/CLI cases | All 15 pass |
| Existing scheduling/trace runs and independent trace reconstructions | Six normal exits and both reconstructions pass |

Direct loader tests link the actual maintained kernel and loader objects, not a replacement allocator or transcribed loader. They run under a minimal deterministic kernel-services fixture with the tested linear page table and FILESYS_STUB configuration. The baseline used the same public Load/Translate APIs and then-current core build; the valid basic load control passed while the targeted checks failed.

Valid-image cases cover short tails, one/two exact pages, nonaligned code/data, multi-page and shared readonly data, sparse layouts and swapped headers. A fragmented frame map reserves every other physical frame with canary bytes, proving that page translation does not assume contiguous or identity mappings. Each successful layout is loaded/reclaimed three times; payload, zero regions, flags, protected writes, unique frames and resident canaries are checked.

Failure checks cover short/bad headers, negative ranges, invalid file offsets, integer-boundary virtual addresses, truncated payloads, virtual overlap, oversized images, exact full capacity, zero capacity, inconsistent counters, already-loaded rejection and invalid translations. An intercepted host read forces a short second payload chunk after reservation; the test verifies rollback, resident mapping/bytes, user PC registers and descriptor counts. The same address-space object is retried after both header-level and allocated short-read failures. Eighty consecutive oversized rejections keep the descriptor count and frame bitmap stable.

The eight MIPS batches are oversized alone; oversized before/after a small valid program; a large resident with a rejected small child; the small program before the resident; two competing large residents; nine large programs sequentially reusing frames; and a malformed image followed by a valid program. The 14,000-byte BSS resident initializes boundary bytes, blocks on PrintInt, and later verifies those bytes and zero BSS before exiting with 77. It survives the other child's allocation failure. The nine-program reuse batch returns 78 nine times. The 20,000-byte BSS program is rejected without executing its negative exit marker. Every case requires expected diagnostics/returns, no assertion, exactly one normal halt and exit 0. The previous shutdown invariant also confirms that all physical frames have returned at automatic halt.

Intermediate setup failures are retained but are not counted as runtime results: a C++ header/min-macro interaction was corrected locally, followed by an explicit header-byte-order fix. The final source was freshly rebuilt without -fpermissive. Existing legacy warnings remain.

## Reproduction and preservation

From a full recovered working clone on Linux:

```sh
bash scripts/verify-memory.sh /path/to/new-results
```

Use a new results directory. Requirements remain a C++11 compiler, make, ed, Python 3, multilib and the i386 runtime for the bundled legacy MIPS tools. The script exports the original recovered commit and overlays the eleven maintained files on Linux-native temporary storage. It builds real host objects, runs loader/accounting/lifecycle suites, forcibly rebuilds the three course MIPS examples and three new pressure fixtures, then runs the existing shutdown and new memory-pressure integrations. Generated artifacts remain outside the tracked sources.

[Portable verification](memory-verification.json) records exact source hashes, before/after outcomes and preservation checks. Raw evidence is archived locally in audit-output/mp3-memory-verified. Historical source reports and JSON files retain their original-stage evidence; they are superseded here for the loader/OOM findings.

The original 291-file recovery snapshot, source ZIP and bundle remain unchanged. The preceding MP1/MP2/MP4 maintained sources remain unchanged. No GitHub update or merge occurs in this local stage.

## Remaining limits

Verification covers simulated-memory allocation and the tested NOFF bounds/short-read failures in the stub-filesystem, linear-page-table configuration. It does not validate a program's instruction semantics, nonempty entrypoint, arbitrary binary formats, TLB operation or real-disk filesystem concurrency. The all-BSS exact-capacity fixture is a loader test, not an executable-program claim. Host allocator exhaustion and host read/seek/close errors that the legacy sysdep layer asserts on remain outside this change. No claim of universal malformed-executable or hardware-fault recovery is made.

User syscall addresses/strings/buffers still need their own MP3-specific validation. General file ownership, recoverable host I/O behavior and broader user-exception handling remain separate audited work. The next priority is the MP3 syscall memory boundary, without importing a different assignment's implementation wholesale.
