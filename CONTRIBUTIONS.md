# Contribution map

Baseline: `3f1ff05f1405b5c992f196f998cfb5756dd68bd4`.

The repository has three import-oriented commits, not a complete development history. An untouched 2023 course starter is unavailable. These entries identify supported contribution areas, not line-by-line authorship or ownership of entire files.

| Module | Contribution area | Implementation locations | Evidence |
| --- | --- | --- | --- |
| MP1 | Assembly syscall stubs and call declarations | `NachOS-4.0_MP1/code/test/start.S`, `code/userprog/syscall.h` | Report implementation section and assignment modification list |
| MP1 | Register arguments, return values, PC advancement, kernel wrappers | `NachOS-4.0_MP1/code/userprog/exception.cc`, `ksyscall.h` | Report and corresponding TODO implementations |
| MP1 | Open-file table operations | `NachOS-4.0_MP1/code/filesys/filesys.h` | Report implementation and code screenshots |
| MP2 | Frame tracking and allocation lifecycle | `NachOS-4.0_MP2/code/threads/kernel.h`, `kernel.cc`; `code/userprog/addrspace.cc` | MP2 annotations and report |
| MP2 | Page mapping, segment loading, frame release | `NachOS-4.0_MP2/code/userprog/addrspace.cc`, `addrspace.h` | Report implementation section |
| MP2 | Additional exception enum | `NachOS-4.0_MP2/code/machine/machine.h` | Explicit assignment requirement and report |
| MP4 | Main lifecycle, Fork/Table synchronization, Philosopher TODO routines | `Dining-Philosopher/main.cpp`, `fork.cpp`, `table.cpp`, `philosopher.cpp` | Report identifies implemented routines |
| MP4 | Attempted turn-based fairness extension | `Dining-Philosopher/table.hpp`, `table.cpp`, `philosopher.cpp` | Report bonus section; correctness not established |

MP4 constructor/think/eat routines and most headers are described in the report as supplied scaffolding. NachOS libraries, simulation, scheduling support, filesystem support, and example programs are primarily framework material with local assignment modifications. The MIPS toolchain is third-party software.

Bundled test programs are not independently established as personally authored. MP1-to-MP2 differences may include starter-version changes and must not be presented wholesale as personal contributions. MP3 has now been recovered from the exact former gitlink target; its one import commit and supplied report support contribution areas, not whole-file authorship.

## Subsequent local maintenance

The documented lifecycle, memory-loader and syscall-copy repairs were made after the course submission, with new regression tests. They must not be presented as functionality verified in the original 2023 submission. See [the latest repair](docs/mp2-syscall-copy-fix.md).

Later [file-resource maintenance](docs/file-io-resource-fix.md) changes the supplied FileSystem/OpenFile stub and adds recoverable sysdep wrappers in both snapshots. These are post-course repairs, not claims about the original student submission or ownership of the framework.

The later [MP1 syscall-boundary validation](docs/mp1-user-memory-fix.md) is also post-course maintenance, distinct from the original submission.

The [MP4 synchronization and safe-stop changes](docs/mp4-lifecycle-fix.md), including changes to supplied constructor/think routines, are post-course maintenance with separate new tests; they do not establish original student authorship of supplied scaffolding.

The subsequent [FIFO fairness repair](docs/mp4-fairness-fix.md) is also post-course maintenance. It changes Table header/API and scheduling implementation; its conditional progress argument must not be attributed to the original submission.

## Recovered MP3 and later maintenance

Original MP3 scheduling work is concentrated in threads/thread.*, scheduler.* and alarm.cc, with CPU burst/remaining-time logic and queue/timer integration. The recovered report and specification were compared locally with these sources. No untouched starter is available, so inherited MP1/MP2 work and framework support cannot be attributed wholesale to the student. The seventeen maintained files and all new regression drivers are subsequent 2026 repairs, not verified functionality of the original course submission. See [MP3 repair and test documentation](NachOS-4.0_MP3/README.md).
