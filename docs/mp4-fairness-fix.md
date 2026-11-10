# MP4 FIFO fork reservations and starvation avoidance

This post-course change replaces the scheduling hint with a FIFO queue of complete two-fork reservations. Production changes relative to the preceding local lifecycle repair are confined to `Dining-Philosopher/table.hpp`, `table.cpp` and `philosopher.cpp`. This is a maintained application variant, not a claim of compliance with the course's header-replacement rule.

## Scheduling policy

A philosopher thinks outside the queue, then calls Table::wait(id). Under one table mutex, its stack-local request is linked at the queue tail. It may proceed only if it is the head and both adjacent forks are unreserved. Granting removes the request and reserves both forks atomically. The next head is notified, allowing a disjoint meal to start while an earlier meal is still running. A conflicting head blocks later requests even if they could use otherwise idle forks; this deliberate head-of-line cost prevents bypass.

The worker then acquires the actual Fork objects, eats, returns both physical forks and finally calls Table::signal(id) to release its reservation. The old turn condition and its skip-meal path are removed. The synchronized turn counter remains for diagnostics and compatibility with the existing counter regression, but cannot authorize or deny a meal. Table stores its actual constructor participant count; ids are checked against that count, including wraparound for the last philosopher.

This changes Table's public wait/signal calls to require an id. Philosopher's public interface stays unchanged. Table copying is prohibited because copying a live pthread mutex/condition/queue would be invalid. Callers must have at most one outstanding request per id, release only an owned reservation, and keep Table alive until all workers are joined. The supplied main enforces this lifecycle. Directly acquiring a physical fork outside this protocol is supported only by controlled tests; an external holder that never releases it invalidates the progress argument.

## Safety and progress argument

The mutex protects the entire queue, per-id state and reservation map. Only a head with two free fork slots can mark them occupied. Therefore two active reservations cannot share a fork. Physical forks are returned before clearing the reservation, so another correctly participating worker cannot compete for a still-held physical fork. Workers never hold a physical fork while waiting in the scheduling queue. This removes circular hold-and-wait among participating philosophers.

Let a non-cancelled request R be linked at the tail. With N philosophers and one outstanding request per philosopher, at most N-1 other requests can already be active or ahead of R. New requests are appended behind R and cannot be granted while R remains queued. Every predecessor is either cancelled and removed, or granted once and then finishes/releases before it can requeue behind R. Once the finite predecessor set has released the forks needed by the next head, that head can proceed. By induction R eventually becomes head and obtains its fork pair. Thus **at most N-1 other reservation grants can occur ahead of R after it is enqueued**, and no later request can bypass it before its grant. Existing active meals may still need to finish; the bound is on reservation grants, not elapsed seconds or the order in which print messages appear.

This starvation-avoidance claim assumes weakly fair CPU/mutex scheduling (runnable contenders eventually run/acquire the lock), finite meal/output/cleanup operations, and eventual release by every active participant. POSIX does not promise FIFO mutex acquisition, so the application queue cannot guarantee a time bound before acquiring the table mutex and joining the queue, or against an OS that indefinitely suspends a worker. There is no real-time deadline or unconditional wall-clock bound. A request cancelled by the caller is removed, not promised a meal. Stress-test counts support the implementation; they do not replace these assumptions or the argument.

The condition variable is only a notification mechanism. POSIX allows spurious wakeups and leaves wake order to the scheduling policy, so every return rechecks the queue-head and pair-free predicates. Release, successful grant and queued cancellation all broadcast under the same mutex, ensuring the eligible head can recheck without depending on which waiter a single signal chooses. See the [POSIX condition-wait specification](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html) and [broadcast specification](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html).

## Cancellation and stopping

The intrusive queue uses stack-local nodes without allocating in the locked wait path. A deferred cancellation at cond_wait first reacquires the mutex, then its cleanup handler unlinks the node (head, middle or tail), resets its id to idle, broadcasts and unlocks. Neighbours cannot retain a pointer to a departed stack node. On a successful grant the same handler only unlocks; the node has already been removed and ownership transferred to the caller.

There is no cancellation point between grant return and marking the Meal record seated. Existing nested cleanup returns a partially acquired fork, then any complete pair, and finally the reservation. Fork release precedes reservation release even on cancellation. The main thread's cancel/join-before-destruction policy is unchanged. The handoff tests cover cancellation before release as well as after successful grant.

## Verification

All results are local WSL/Linux runs; original snapshots, reports, configuration and tracked binaries remain unchanged.

| Suite | Result | Evidence directory outside repository |
| --- | --- | --- |
| Fairness functional | 13 cases pass | `../../audit-output/mp4-fairness-delivery/native/` |
| Fairness ThreadSanitizer | 8 scoped cases pass | `../../audit-output/mp4-fairness-delivery/tsan/` |
| Existing lifecycle regression | 24 cases pass, including default 15-second application and creation failures | `../../audit-output/mp4-fairness-lifecycle-delivery/` |
| Complete worker ThreadSanitizer | 2 scoped cases pass | `../../audit-output/mp4-fairness-worker-races-delivery/cases/` |

Fairness cases verify queued FIFO order against immediate re-entry, injected spurious returns, disjoint meals overlapping, 300 meals per participant for N=2/3/5/7 with one faster requester, invalid ids, head/middle/tail/all-waiter cancellation, tail reuse, and 200 cancellation/grant handoffs. Test workers also track fork masks to reject overlapping conflicting reservations and probe every reservation after joins. Queue arrival is observed through test-only cond_wait wrapping rather than access to private queue state. The existing lifecycle suite was adapted from capacity saturation to conflicting id-aware reservations; it still covers thinking, table wait, each physical-fork wait, eating, cleanup, parameter counts and startup failure.

Forced cancellation is tested natively and with the lifecycle suite's ERRORCHECK mutexes. It is excluded from the eight clean fairness TSan cases because the prior independent probes exposed cancellation-analysis limitations. The complete-worker TSan workload uses pthread_exit outside the libc sleep interceptor, as before. Retained forced-cancellation diagnostics and the earlier minimal reproductions remain documented in [the lifecycle report](mp4-lifecycle-fix.md); this change does not claim that those analyzer limitations have disappeared. Default make builds with warnings enabled and no warnings.

From the repository on Linux, use new output directories:

```sh
bash scripts/test-mp4-fairness.sh ../audit-output/fairness-new
bash scripts/test-mp4-lifecycle.sh ../audit-output/lifecycle-new
bash scripts/test-mp4-races.sh ../audit-output/races-new
```

The new Table API requires the maintained headers and sources together. Current lifecycle/race drivers cannot compile against the original no-id Table API; NACHOS_MP4_BASELINE=1 now exits explicitly instead of pretending a valid baseline comparison. Historical baseline logs remain available from the earlier audit. No original reports have been edited to match the repaired program.

## Remaining limits

MP4-02 is locally addressed through FIFO bounded bypass with the stated progress assumptions. This is not maximum-throughput scheduling: head-of-line blocking may leave usable forks idle. MP4-04 course starter-header compatibility remains unresolved, and the explicit Table API extension must be reviewed if strict course grading compatibility becomes a requirement. License, attribution and original-report publication checks are unchanged. These results were obtained before the maintenance branch was published.
