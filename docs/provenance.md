# Local preparation provenance

Original remote: https://github.com/HLin34/NachOS-4.0

Baseline: `3f1ff05f1405b5c992f196f998cfb5756dd68bd4`.

Local backup: `../nachos-backup/original.bundle`, verified with `git bundle verify` on 2026-11-02. SHA-256: `15465a8839fdd836d3c692f21d63464b61fa12341fe9a2e71d9ed15090b176a4`.

`../nachos-backup/source-manifest.json` records the original tracked-file hashes and unresolved MP3 gitlink. The bundle contains all available Git history, including tracked PDFs and toolchain files, but cannot contain the absent MP3 target object.

This preparation directory was cloned from the verified bundle. Its local origin was removed to avoid an accidental push target. The audit source directory remains unchanged. Baseline compilation uses a separate export in `../nachos-build-baseline`, so generated files cannot contaminate the original snapshot or documentation changes.

At the end of the local audit stage, no remote changes had been performed. Backup files include original identifying information and are local archival material, not public deliverables.

On 2026-11-09, four MP2 source files in the preparation directory received the documented batch-lifecycle repair. Original files remain available in nachos-audit and the verified bundle. See [repair scope and tests](mp2-shutdown-fix.md).

The same four MP2 source files now also contain the documented [frame allocation and loader repair](mp2-memory-fix.md). No original PDF or machine-emulation source was changed.

The subsequent [syscall memory-copy repair](mp2-syscall-copy-fix.md) adds changes to MP2 code/userprog/exception.cc and code/test/start.S. At that stage the preparation directory differed from the original in six MP2 source files; the original snapshot and bundle remain unchanged.

The [memory-limit repair](mp2-memory-limit-fix.md) updates addrspace.cc, exception.cc, and the ForkExecute cleanup comment within the same six-file repair set. Original files and reports remain unchanged in the preserved audit snapshot.

The [file-resource repair](file-io-resource-fix.md) adds changes to filesys/filesys.h, filesys/openfile.h, lib/sysdep.h and lib/sysdep.cc in each assignment. At that stage the source change set was fourteen files: four MP1 and ten MP2. All original notices and the preserved snapshot remain intact.

The [MP1 user-memory repair](mp1-user-memory-fix.md) adds only MP1 code/userprog/exception.cc. At that stage the source difference from the original was fifteen files: five MP1 and ten MP2. The previous fourteen source repairs are unchanged.

The [MP4 synchronization/stop repair](mp4-lifecycle-fix.md) adds five files in Dining-Philosopher. At that stage the tracked source difference was twenty files: five MP1, ten MP2 and five MP4. The preceding fifteen MP1/MP2 repairs remain byte-for-byte unchanged. Original headers, configuration, binaries and PDFs remain intact.

The subsequent [FIFO fairness repair](mp4-fairness-fix.md) revises MP4 table.cpp/philosopher.cpp and adds table.hpp to the maintained change set. The current tracked source difference is twenty-one files: five MP1, ten MP2 and six MP4. All fifteen MP1/MP2 repairs and the preceding MP4 fork.cpp/main.cpp/Makefile repairs remain unchanged. The original audit snapshot, all PDFs, config, other headers and tracked executables remain unchanged.

## Maintenance publication

The publication contains the twenty-one previously verified source changes plus documentation and regression tooling. It preserves the historical directory layout, all tracked binaries/PDFs, existing notices and the exact MP3 gitlink. A new branch and pull request carry the update; no default-branch history rewrite, visibility change, new license or regenerated archival material is part of this publication. Machine-specific backup paths and historical evidence references describe local archival context, not files required in a fresh clone. See [the published verification summary](verification-summary.md) for portable result records and source hashes.

## MP3 recovery and integration

The earlier missing-target findings above describe the first maintenance publication. A user-supplied backup subsequently recovered the exact MP3 target and its complete nested history. Both the original recovered repository and the maintained working tree have verified local backups. MP3 is imported as ordinary text source, tests and repair documents; the old gitlink remains visible in earlier main history. Newly imported PDFs, nested metadata, duplicate compiler packages and generated binaries are excluded. Existing MP1/MP2/MP4 blobs remain intact. See [current recovery status](mp3-status.md).
