# MP3 safe batch completion and shutdown — 2026-11-10

**Integration note:** This report records its original local repair stage. For the current ordinary-directory layout, GitHub publication and portable commands, see [integration details](integration.md).

**Subsequent file-I/O repair:** [recoverable I/O and descriptor ownership/cleanup](file-io-fix.md) supersedes the corresponding open limitations in this historical report. Current verification scripts overlay seventeen maintained source files; original-stage hashes remain preserved evidence.

**Subsequent syscall-boundary repair:** [paged address/string/buffer validation](user-memory-fix.md) now addresses the implemented syscall memory boundary described as open in this historical report. Current scripts overlay twelve files; earlier source hashes and results remain the evidence for their original stage.

**Subsequent memory repair:** [cross-page loading and memory-shortage recovery](memory-fix.md) now supersedes the loader/OOM limits described in this historical report. Current scripts overlay eleven files; original-stage result records below remain unchanged.

The scheduling-repaired simulator returned the expected user results but never stopped. Periodic timer and console events keep the pending-interrupt queue nonempty; Interrupt::Idle therefore cannot reliably infer that a user batch has finished. This stage tracks the batch's user-program lifetime instead of waiting for hardware events to disappear.

## Lifecycle policy

Kernel initializes its launch counters, registration table and owned-service pointers deterministically. Main uses ID 0. A CLI batch has at most nine user programs, preserving the existing ten-slot thread-table capacity. Executable arguments are stored with zero-based indexing. Missing/option-shaped -e operands and a tenth program are rejected before initialization or any partial launch; filenames beginning with a dash can use a relative ./ prefix. The thread lookup also rejects out-of-range ids.

Exec reserves the id and registers the live child with interrupts disabled, before Fork can hand off execution. A launch-in-progress guard remains set until every command has been submitted. An early completion cannot stop a partially launched batch. A registered child stays live while READY, RUNNING or BLOCKED on I/O; the size of the ready queue is never used as a completion condition.

Thread::Finish disables interrupts and settles its final CPU slice before cleanup. It clears the simulated user mapping, detaches and deletes its address space, then notifies Kernel. AddrSpace initializes numPages for failed-open cleanup and uses delete[] for its page-table array. Its existing destructor returns allocated frames. The same path handles a normal Exit and a missing executable that causes ForkExecute to return. A matching registration may complete only after space is detached; clearing the registration makes duplicate notifications harmless. Main and foreign thread pointers cannot consume a user's registration.

Only after launching is complete and the live count reaches zero does the kernel halt. An empty or already-completed batch follows the same path. Before automatic halt, invariants require all registrations cleared and all physical frames available. Statistics remain alive through service destruction; callbacks are disabled during teardown and pending event records are discarded without being invoked. The current thread's stack is not deleted while running: earlier threads retain normal deferred destruction, while the final stack is reclaimed when the host process exits.

The scheduling comparator, timer-preemption decision, burst training recurrence and hardware simulator are unchanged. Finish-time settlement is idempotent with the settlement already performed by Sleep, preserving the final CPU slice even when last-child halt bypasses Sleep.

## Executed verification

A fresh native-Linux export rebuilt the host without -fpermissive, forcibly rebuilt all three MIPS examples, and ran the following suites:

| Suite | Result |
| --- | --- |
| Existing real Thread/Scheduler/Alarm focused tests | 21 pass |
| New batch notification/launch boundary tests | 7 pass |
| Real MIPS / CLI shutdown cases | 15 pass |
| Existing six scheduling/trace program runs | All now exit 0 with exactly one halt |
| Independent burst and preemption trace reconstructions | Both pass; original hw3t1-first behavior preserved |

The seven lifecycle cases cover empty completion; finishing inside each Fork before later submissions; multiple live children including an explicitly BLOCKED child; duplicate notifications; foreign/main notifications; nine reserved ids; and a standalone completed child before ExecAll starts the batch. These link actual Kernel and AddrSpace objects. Test-only wrappers model Fork completion and intercept the nonreturning main Finish/Halt boundary; they are event-boundary tests, not native concurrency or full user execution. The separate MIPS suite exercises real dispatch, I/O waiting, load failures and destruction.

The fifteen integration cases cover an empty batch, one program, both pair launch orders, random timer seeds 7 and 23, a missing executable, missing-then-valid, valid-then-missing, all missing, nine programs, missing -e operand, option mistaken for an operand, over-capacity rejection, and redirected console output. Each normal case requires the exact expected return sequence, exit 0, exactly one halt and no assertion. Parse failures require exit 1, the expected diagnostic, no user returns and no halt. Redirected output must contain every byte of both PrintInt calls after the output device closes. This checks that final I/O is not truncated by a premature automatic halt.

The earlier scheduling-stage evidence records all six corresponding runs timing out after user returns. The maintained runs now end normally. The two trace reconstructions still validate running intervals, estimates, queue order and strictly-shorter preemption. Both launch orders return 1 before 2; reverse trace still includes three real timer preemptions.

## Scope and remaining limits

This stage modifies kernel.cc/kernel.h and Finish within thread.cc, plus the minimal AddrSpace constructor/delete[] cleanup prerequisite. The cumulative original-file change set is ten files; previous list/dependency repairs and the scheduler policy methods remain unchanged. No machine source, PDFs, compiler or historical executable is modified. The original recovered snapshot/bundle and the 21 earlier MP1/MP2/MP4 maintained source files remain intact. GitHub has not been updated in this local stage.

The policy is one bounded CLI batch with monotonically reserved ids, not a reusable process service or a new Join/exit-status system. A completed batch containing missing executables still exits 0 after printing the load errors, following the existing host exit-status policy; this does not mean those programs loaded successfully. Nonzero user Exit values are still printed rather than becoming the host exit code. Explicit user Halt retains its immediate whole-machine meaning and is outside the automatic last-child guarantee.

Malformed executables, LoadData segment boundaries, simulated/host allocation failure, generic exception handling and broad file ownership remain separate work. In particular, the previously identified loader counterexamples and OOM assertion path are not repaired here. This stage verifies resource cleanup for the tested normal and failed-open paths, not every possible load failure. Other legacy CLI options retain their previous parsing behavior.

## Reproduction

From this full working repository on Linux:

```sh
bash scripts/verify-shutdown.sh /path/to/new-results
```

The output directory must be new. Requirements remain g++, make, ed, Python 3, multilib and the Linux i386 runtime for the legacy MIPS tools. The script exports the recovered commit and overlays the ten maintained files, excluding archival executables from build success. Generated build products stay in native temporary storage. Existing legacy warnings remain.

The final run is archived locally under audit-output/mp3-shutdown-verified. [Portable verification](shutdown-verification.json) records source hashes, all suites and preservation checks. Historical build and scheduling reports describe their earlier stages; this report supersedes their shutdown-timeout findings for the maintained version.
