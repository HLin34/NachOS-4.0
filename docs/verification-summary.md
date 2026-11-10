# Verification summary

Results were obtained during the November 2026 maintenance work in Ubuntu WSL2 with GCC 15.2. [Portable result records](verification/results.json) include per-case status, the baseline SHA and LF-normalized hashes of the 21 maintained source files. Raw build/runtime logs remain in the local audit archive; absolute machine paths and generated binaries are not published.

| Module | Executed evidence |
| --- | --- |
| MP1 | 20 file-layer cases; 28 user-memory cases; real MIPS boundary recovery; three file recovery/bundled-example runs |
| MP2 | 11 loader/frame cases, 17 syscall cases, three MIPS syscall runs, eight pressure cases, five MIPS pressure batches and seven shutdown cases (51); 20 file-layer cases; MIPS file-error recovery |
| MP4 | 13 fairness functional cases, 24 lifecycle cases, eight scoped fairness TSan cases and two scoped complete-worker TSan cases |

These are focused checks, not a claim of hidden-test coverage or universal correctness. MP1/MP2 results were rerun at their final repair stages; the source hashes and subsequent preservation checks establish that MP4 work did not alter those files. MP4 results use the final FIFO variant. Five workers each completed 15 meals in the complete-worker workload.

The original MP2 shutdown control is deliberately recorded as failed/timed out; it demonstrates the repaired trigger. All maintained functional result rows passed. Scoped TSan workloads passed, while known forced-cancellation diagnostics are retained in the local archive and discussed in [the lifecycle report](mp4-lifecycle-fix.md). They are not counted as clean tests. Minimal diagnostic reproducers are supplied in tests; no blanket sanitizer-clean claim is made.

Reproduce with [the build/test commands](build.md). The historical baseline hash must remain available in a full clone. MP3 was subsequently recovered and verified as described below. FIFO progress requires eventual CPU/mutex scheduling and finite holding of resources; strict course starter-header compatibility remains unresolved. Historical report screenshots do not replace executable verification.

## Recovered MP3

[MP3 per-case records and seventeen source hashes](../NachOS-4.0_MP3/docs/file-io-verification.json) record 35 file-I/O/resource cases, four real MIPS ownership/capacity/reuse runs, 39 syscall-boundary cases, three syscall MIPS recovery runs, 27 loader cases, eight pressure runs, 21 scheduler cases, seven lifecycle cases, fifteen shutdown cases, six shutdown-trace checks and eight scheduling/trace records. All passed. The integrated publication is additionally built and tested from an independent clone before merge; [publication verification](mp3-publication-verification.json) records its result. These are focused scoped tests, not a certification of all OS features or every host failure. The original MP1/MP2/MP4 source and result records remain unchanged.
