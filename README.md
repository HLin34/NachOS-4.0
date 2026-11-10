# NachOS 4.0 — Operating Systems Coursework

Coursework and subsequent maintenance covering system calls, paged address spaces, multiprogramming and POSIX thread synchronization. The original teaching framework and course scaffolding are preserved alongside assignment implementations.

| Module | Focus | Maintained status |
| --- | --- | --- |
| [MP1](NachOS-4.0_MP1/code/) | File system calls and user/kernel memory boundaries | File-resource, I/O-error and bounded memory-copy repairs with direct and MIPS integration tests |
| [MP2](NachOS-4.0_MP2/code/) | Physical frames, page tables, segment loading and multiple programs | Loader, memory-pressure recovery, syscall copying and batch shutdown repairs with regression tests |
| MP3 | Missing nested Git repository | Original gitlink retained; target content could not be recovered. See [recovery status](docs/mp3-status.md) |
| [MP4](Dining-Philosopher/) | Native pthread dining philosophers | Cancellation-safe stopping and FIFO fork-pair reservations, with functional and scoped race checks |

## Original submission and later repairs

The original submission is preserved in Git history at `3f1ff05f1405b5c992f196f998cfb5756dd68bd4`. Maintenance fixes and new regression tests were developed separately in November 2026; their results must not be attributed to the original coursework. [Contributions](CONTRIBUTIONS.md) distinguishes assignment work, supplied components and attribution limits.

MP2 eagerly allocates pages; it is not a demonstrated demand-paging or swapping system. MP4 runs independently of the NachOS simulator. Its FIFO policy prevents queued-request bypass and avoids starvation under eventual scheduling and finite resource-holding assumptions; it has no unconditional time bound. The maintained Table API differs from the course header interface.

## Build and verify

MP1/MP2 need Linux with 32-bit development/runtime support, GNU make, ed, Python 3 and the bundled legacy MIPS tools. Modern host compilers require compatibility flags. The legacy compiler must run on Linux-native storage rather than a Windows-mounted path; the supplied regression scripts prepare separate exports for this.

MP4 uses a native C++11 compiler with pthread support:

```sh
make -C Dining-Philosopher
bash scripts/test-mp4-fairness.sh /tmp/mp4-fairness-results
bash scripts/test-mp4-lifecycle.sh /tmp/mp4-lifecycle-results
```

Test result directories must be new. Run scripts with bash. Use a full Git clone: several MP1/MP2 regression scripts export the historical baseline from Git history. See [build instructions](docs/build.md), [verification summary](docs/verification-summary.md) and [known issues](docs/known-issues.md). Tracked prebuilt files are historical artifacts and are not evidence of a clean source build.

## Repository organization

The original MP1/MP2 and Dining-Philosopher layouts remain intact so reports can be compared with their sources. `docs/` records repair policies, provenance, verification and unresolved findings; `tests/` holds new regression drivers; `scripts/` builds and runs isolated test copies. MP3 remains an unresolved gitlink, not an empty implemented assignment.

Repair details: [MP2 shutdown](docs/mp2-shutdown-fix.md), [frame/segment loading](docs/mp2-memory-fix.md), [syscall copying](docs/mp2-syscall-copy-fix.md), [memory exhaustion](docs/mp2-memory-limit-fix.md), [MP1/MP2 file resources](docs/file-io-resource-fix.md), [MP1 memory validation](docs/mp1-user-memory-fix.md), [MP4 stopping](docs/mp4-lifecycle-fix.md), and [MP4 fairness](docs/mp4-fairness-fix.md).

## Provenance and limitations

Existing framework notices are preserved; no blanket license is added. Course-material distribution terms and complete legacy-toolchain provenance remain unresolved. Existing historical PDFs contain identifying information and remain unchanged in this maintenance update; no new PDF derivatives or toolchain packages are added. See [third-party notices](THIRD_PARTY_NOTICES.md) and [provenance](docs/provenance.md).

MP3 recovery, strict course-header compatibility, process-specific file ownership and broader malformed-executable handling remain outside the completed repairs. Forced-cancellation race-analyzer limitations are recorded explicitly; passing scoped checks does not certify all execution paths.
