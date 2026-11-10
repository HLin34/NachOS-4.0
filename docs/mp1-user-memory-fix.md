# MP1 user-address, string and buffer validation

## Local repair

Create, Open, Read, Write and diagnostic MSG no longer pass user register values directly to host-memory/file routines. The MP1 exception handler now uses the same bounded page-table validation and copy semantics already tested in MP2. The assignments retain their separate source snapshots; MP2 source was not changed in this step.

Every nonempty buffer must fit completely inside the active virtual page table. Each covered entry must be valid and map to an in-range physical frame; a Read destination must also be writable. Checks use subtraction before addition to reject overflowing ranges. The whole requested span is checked before file I/O, so rejection cannot consume file bytes or overwrite a valid prefix before encountering a bad later page.

Copying proceeds up to each virtual page boundary, allowing adjacent virtual pages to occupy different physical frames. Copy-in sets use; copy-out sets use and dirty. Read copies only its actual result, preserving destination bytes beyond EOF. Negative/oversized counts are rejected. Zero-byte operations do not dereference the pointer, but file-ID validation still applies.

Filename and diagnostic strings need a NUL within 256 bytes; the maximum non-NUL path length is 255 bytes. A NUL at the final valid virtual byte is accepted. Invalid Open/Read/Write and invalid MSG return -1. Invalid Create preserves its existing failure value 0. Error returns advance the program counter normally. A valid MSG retains its original print-and-Halt behavior.

Virtual address zero is not automatically rejected: NachOS places code at virtual zero, so mapping/protection flags determine validity. This step enforces existing readOnly flags; it does not add read-only segment markings to the original MP1 loader, which initializes its entries writable. It also does not add multiprogramming, TLB handling, or previously unsupported syscall implementations to MP1.

## Evidence

The direct driver links the real MP1 exception handler and filesystem. Artificial fragmented mappings isolate pointer handling, including switching between two mappings; this is a test of the handler, not a claim that MP1 acquired MP2's multiprogramming implementation. The test driver supplies zeroed Kernel backing storage to isolate this work from the original constructor's unrelated counter-initialization issue; the constructor is unchanged.

The original handler fails 21 of 28 direct cases with the existing file-layer repair held constant. The other seven controls pass. All 28 pass after this repair. Cases cover cross-page names/data, read-only destinations, invalid page/frame/table metadata, negative/oversized inputs, zero length, EOF tails, exact buffer endings, 255/256-byte string limits, invalid diagnostic strings, and rejecting a bad later page before changing a valid prefix or existing file contents.

Evidence: `../../audit-output/mp1-user-memory-before/cases/results.json` and `../../audit-output/mp1-user-memory-verified/cases/results.json`.

A separately compiled MIPS program issues negative-pointer and oversized-count requests, overlong names and invalid MSG, then successfully creates a file through a cross-page filename and writes/reads/compares 300 bytes across page boundaries. Its EOF tail remains unchanged. The runner requires its success marker, one normal Halt, no assertion, and exact host file contents. Evidence: `../../audit-output/mp1-user-memory-verified/mips/results.json`.

The prior file-error recovery program and bundled MP1 write/read pair were rerun successfully against the repaired handler. The same integration command's MP2 control also passes. Evidence: `../../audit-output/mp1-user-memory-file-regression/{mp1,mp2}/results.json`. The fourteen previously changed source files match the preceding delivery; this step adds only MP1 code/userprog/exception.cc to the source change set.

## Reproduce in Ubuntu

Use new output directories from the preparation directory:

```sh
bash scripts/test-mp1-user-memory.sh ../mp1-boundaries-new
NACHOS_MP1_BASELINE=1 bash scripts/test-mp1-user-memory.sh ../mp1-boundaries-before-new
bash scripts/test-file-io-integration.sh ../mp1-file-regression-new
```

Baseline mode retains the original MP1 handler and runs the direct cases; a nonzero result is expected. Normal mode overlays five current MP1 source files, builds on Linux-native storage, runs the 28 direct cases, and compiles/runs the MIPS recovery fixture. The course's single-program Halt convention is preserved.

The original audit snapshot and GitHub remain unchanged. Separate remaining work includes MP4 synchronization, shared/per-process file ownership, broader loader validation, and the MP1 constructor counters noted above. These focused results are not a certification of all course hidden tests.
