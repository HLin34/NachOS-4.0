# Third-party provenance inventory â€” draft

This is an inventory, not a replacement license or a completed redistribution review.

| Component | Existing evidence | Remaining work |
| --- | --- | --- |
| NachOS framework | Assignment-root COPYRIGHT files and `code/lib/copyright.h`; UC Regents notices in source | Preserve all existing notices when reorganizing |
| Kernel syscall interface | Marcus Voelp / Universitaet Karlsruhe attribution in ksyscall.h | Retain attribution and establish exact supplied version |
| Legacy MIPS compiler and utilities | `usr/local/nachos`, GCC 2.95.2 path, toolchain archive | Record upstream source, complete license notices and installation requirements before repackaging |
| Course scaffolding and examples | Assignment specs, TODOs, reports | Recover starter baseline and establish public distribution scope |
| Course PDFs | Historical specs and reports in the original snapshot | Review publication scope and remove identifying/internal information in any public derivative |

No blanket MIT or other new license is applied to the repository. The existing framework notices remain authoritative for their covered material. Course publication conditions and toolchain provenance remain unresolved; local preparation can proceed while these are investigated.


The recovered MP3 import retains its COPYRIGHT and source notices, including UC Regents and the kernel syscall interface attribution. It introduces no new PDF or legacy binary package: its compiler/converter are reused from the existing identical MP2 snapshot. Recovery of the original commit does not establish a new blanket license or complete course/toolchain redistribution provenance.
