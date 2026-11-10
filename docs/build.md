# Building and verification

## Maintained MP1 and MP2

Use Linux (validated on Ubuntu under WSL2), GNU make, ed, Python 3, g++ and 32-bit C++ development/runtime support. The bundled MIPS GCC 2.95.2 and prebuilt COFF-to-NOFF converter are i386 Linux ELF executables requiring `/lib/ld-linux.so.2`. They were used as supplied binaries, not rebuilt from source. Host compilation was verified with `CC='g++ -fpermissive -std=gnu++11'`; compatibility flags do not certify correctness.

Clone with full Git history. Run from the repository root, choosing new output directories:

```sh
bash scripts/test-file-io.sh /tmp/file-io-results
bash scripts/test-file-io-integration.sh /tmp/file-io-integration-results
bash scripts/test-mp1-user-memory.sh /tmp/mp1-memory-results
bash scripts/test-mp2-memory.sh /tmp/mp2-memory-results
bash scripts/test-mp2-syscalls.sh /tmp/mp2-syscall-results
bash scripts/test-mp2-oom.sh /tmp/mp2-pressure-results
bash scripts/test-mp2-shutdown.sh /tmp/mp2-shutdown-results
```

Scripts export the fixed original commit, overlay explicitly listed maintained files and build on Linux-native temporary storage. They regenerate dependencies, relocate compiler search prefixes and leave tracked prebuilt files untouched. The legacy compiler failed to read its specs on a Windows-mounted filesystem with `Value too large for defined data type`; keep its execution on native Linux storage. Verify disk space and the listed prerequisites before running these builds.

## MP4

MP4 is a separate pthread program, not a NachOS user program. It requires a C++11 compiler, pthread support and make. Regression tests also require Python 3 and GNU linker wrapping.

```sh
make -C Dining-Philosopher
bash scripts/test-mp4-fairness.sh /tmp/mp4-fairness-results
bash scripts/test-mp4-lifecycle.sh /tmp/mp4-lifecycle-results
bash scripts/test-mp4-races.sh /tmp/mp4-race-results
```

The last command requires ThreadSanitizer support and a compatible address-space layout; startup failure is unavailable, not passing. Fairness testing includes a separate scoped TSan run. Forced-cancellation analyzer limitations are documented in [the stop repair](mp4-lifecycle-fix.md). The maintained id-aware Table API must be built with its matching header; current MP4 drivers reject the old baseline flag because the original no-id interface is incompatible. See [fairness policy and progress assumptions](mp4-fairness-fix.md).

## Historical baseline

The baseline `3f1ff05f1405b5c992f196f998cfb5756dd68bd4` intentionally retains the original defects:

```sh
NACHOS_COMPAT=1 bash scripts/verify-baseline.sh /tmp/nachos-baseline-results
```

This destination must be new and on Linux-native storage. Without NACHOS_COMPAT=1, strict modern-host compilation is retained for comparison. The original dependent-base template lookups fail strict compilation; compatibility builds passed after rebuilding old dependency paths and relocating legacy MIPS search prefixes. The original MP1 bundled write/read pair exited normally. The original MP2 pair printed expected output and return messages but timed out after 20 seconds; it is a reproduced failure, not a passing baseline.

The separate shutdown regression expects that baseline failure and verifies normal termination in the repaired copy. Other focused repairs cover page boundaries, nonidentity mappings, error recovery and memory pressure. See [the verification summary](verification-summary.md), repair-specific reports and [known limits](known-issues.md). Historical local log names in those reports identify archived evidence; they are not public filesystem links or dependencies. Portable result records are included under docs/verification.
