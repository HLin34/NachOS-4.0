# MP4 synchronization and safe stopping

**Historical stage:** the subsequent [FIFO fairness repair](mp4-fairness-fix.md) supersedes the N-1 semaphore and turn hint described below, extends table.hpp with id-aware reservations, and adapts these regression drivers. The original baseline flag is no longer supported by the current drivers. Results below remain the retained evidence for the preceding lifecycle repair.

This is post-course local maintenance of five files in `Dining-Philosopher`: fork.cpp, table.cpp, philosopher.cpp, main.cpp and Makefile. Original headers, config, tracked executable, reports and GitHub remain unchanged.

## Changes and ownership

The original currentTurn getter/updater raced. Both now use the table mutex, and each scheduling decision uses one locked snapshot. Negative remainder and hardcoded-five neighbor logic are replaced with configured wraparound. The snapshot remains only a scheduling hint: it neither reserves a turn nor proves starvation freedom.

The stop path no longer accesses the shared cancelled flag. That existing header member is initialized once and otherwise unused. Each worker owns its random-generator state, using rand_r instead of shared srand/rand state. Parameter assertions reject invalid configured counts and intervals. Table rejects fewer than two participants.

Workers use deferred pthread cancellation. Three cleanup layers preserve ownership: a condition waiter unlocks its mutex; a worker waiting for its second fork returns its first fork; and a seated worker returns acquired forks and its table permit. There are no cancellation points between successful acquisition/release and updating the corresponding ownership flag. Cleanup handlers execute in reverse registration order. POSIX condition-wait cancellation reacquires the mutex before invoking cleanup, so the waiter handler must unlock it. See [condition-wait semantics](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3.html) and [cleanup semantics](https://man7.org/linux/man-pages/man3/pthread_cleanup_push.3.html).

Main constructs resources before starting workers, records the successfully started prefix, requests cancellation, joins every started worker, and only then destroys shared objects. A creation failure follows the same cleanup path and returns failure. start retains its void signature and throws system_error on creation failure. An unexpected cancel/join failure terminates the process without unwinding shared resources that a live worker could still reference. Mutex/condition initialization failures roll back initialized components; invariant failures abort. A cancellation request alone does not establish completion; [pthread_cancel](https://man7.org/linux/man-pages/man3/pthread_cancel.3.html) documents joining to establish termination.

Supported lifecycle is one start per Philosopher and cancel/join only for successfully started, owned workers. The application enforces this; the unchanged class interface does not add a general reusable thread-state machine. Makefile now honors compiler/linker overrides, enables C++11, warnings and pthread support, and rebuilds when headers change.

## Executed checks

Linux/WSL functional results are in `../../audit-output/mp4-lifecycle-errorcheck/`. All **24** cases passed: eight core scenarios (thinking, table wait, first-fork wait, second-fork wait, eating, concurrent stress, turn updates and invalid table size), ten additional stress runs, two- and seven-participant runs, three application runs (normal and first/third thread-creation failure), and the unmodified default 15-second application. Accelerated tests use temporary configuration copies and ERRORCHECK mutexes. They join before probing recovered fork/permit capacity and check destruction after worker completion. The default source Makefile builds without warnings. The untouched baseline fails all eight core scenarios; these failures include downstream cleanup assertions, not eight independent proven root causes.

ThreadSanitizer results in `../../audit-output/mp4-races-final-checked/cases/` pass both concurrent turn updates and the complete worker workload. The latter exits workers via pthread_exit outside the sleep interceptor after repeated iterations, exercising cleanup without forced cancellation inside libc sleep. Original turn updates produced an actual race report in `../../audit-output/mp4-races-before/`.

**Forced-cancellation analyzer diagnostics remain recorded, and are not counted as clean runs.** The fixed cancellation stress still reports synchronization anomalies under ThreadSanitizer and Helgrind. A minimal program with a consistently mutex-protected integer reproduces the TSan report (`../../audit-output/mp4-sanitizer-probe/`). An independent condition-wait cancellation probe passes native ERRORCHECK ownership/unlock checks but Helgrind reports an unlocked-lock anomaly (`../../audit-output/mp4-cond-cancel-probe/`). This supports an instrumentation-limit interpretation, consistent with [upstream cancellation/interceptor issue 646](https://github.com/google/sanitizers/issues/646); it is not a proof that every diagnostic is harmless. Helgrind turn updates pass; its cancellation stress does not. DRD also reports a libc output-buffer diagnostic. Raw checker results are retained without suppressions; no blanket sanitizer-clean or race-freedom claim is made. Functional cancellation coverage and scoped clean TSan runs are separate evidence.

## Reproduction

From this repository on Linux, choose new result directories:

```sh
bash scripts/test-mp4-lifecycle.sh ../audit-output/mp4-lifecycle-new
bash scripts/test-mp4-races.sh ../audit-output/mp4-races-new
```

The scripts export into native temporary storage and leave tracked binaries/config unchanged. GCC pthread support, Python 3 and GNU linker wrapping are required; TSan additionally requires a supported runtime/address layout. Startup failures are reported as unavailable, never passing. Set NACHOS_MP4_BASELINE=1 for the untouched baseline. The TSan Python runner also supports --cancel-diagnostic to retain the separate forced-cancellation diagnostic. Optional Helgrind/DRD scripts accept an independently supplied local Valgrind runtime; that runtime is outside the repository and is not a build dependency.

## Remaining scope

MP4-01 is locally repaired with the verification limits above; MP4-03 parameter/wrap issues are locally repaired. MP4-02 starvation freedom remains open: N-1 admission prevents the simple all-left-forks deadlock but does not establish bounded waiting. MP4-04 starter-header compatibility remains unresolved because the original starter header is unavailable. Existing headers were preserved here; no claim of passing the course header-replacement rule is made. Historical report claims are preserved as submitted, with corrections documented here.
