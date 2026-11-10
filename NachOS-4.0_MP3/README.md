# NachOS MP3 — recovered CPU scheduling coursework

**Integrated layout:** MP3 is an ordinary directory. Verification scripts reuse the identical tools already tracked under MP2. See [integration and provenance](docs/integration.md).

Recovered original commit: `57e8e4e99d06b5ff2a268524887d22a3932d611a`, matching the parent repository's previously unresolved gitlink. Original source, reports and tooling are preserved separately in a verified local bundle and manifest.

This working copy includes build, CPU scheduling, safe batch shutdown, cross-page loading/memory recovery **syscall address/string/buffer validation**, and **recoverable file I/O with descriptor ownership/cleanup**. Its seventeen-file maintained set checks paged inputs before file effects, returns actual transfer/error results, and reclaims user-owned files before batch completion.

## Verify the build

From a full clone on Linux with g++, make, ed, Python 3 and 32-bit C++ development/runtime support:

```sh
bash NachOS-4.0_MP3/scripts/verify-file-io.sh /path/to/new-results
```

The output directory must not exist. The script exports the maintained MP3 subtree into Linux-native temporary storage, overlays the seventeen maintained files, regenerates dependencies, removes the archival executable from the build path, and builds a fresh 32-bit simulator. It then recompiles hw3t1/hw3t2/hw3t3 with the bundled legacy MIPS compiler and records bounded smoke observations.

The workflow verifies 35 file-I/O/resource cases and four real MIPS ownership/reuse runs, plus all 39 syscall-boundary cases, three earlier MIPS recovery runs and loader, pressure, scheduling and shutdown regressions. See [file I/O and resource recovery](docs/file-io-fix.md), [syscall memory validation](docs/user-memory-fix.md), [memory loading/recovery](docs/memory-fix.md), [safe shutdown](docs/shutdown-fix.md), [scheduling](docs/scheduling-fix.md) and [the earlier build repair](docs/build-fix.md).

Unsupported user exceptions/syscalls, host-device failures, filesystem path confinement and the separate simulated-disk configuration remain outside this maintenance scope. SJF remaining-time comparison, burst accounting and queue-key update order have the focused repairs described above. This stage changes no machine emulation, PDFs, compiler binaries or historical user-program binaries. Existing notices remain in force; no new blanket license is assigned. The earlier local stage is now integrated through a separate branch and pull request.
