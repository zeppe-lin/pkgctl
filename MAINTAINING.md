# Maintaining pkgctl

## Authority review

Before accepting a feature, identify which component owns every consumed or
produced fact. Reject changes that make `pkgctl` parse, infer, recompute, or
serialize a value already owned by another library.

Controller-owned policy must remain visibly separate from authority results.
Defaults that can remove packages, mutate filesystems, initialize state, or
publish state are prohibited.

## Dependency direction

The executable directly depends on the exact libraries whose public values it
uses. The internal `pkgctl-core` library is not installed and does not publish a
second package-management API.

The construction layer may depend on source, fetch, build, build-exec, image,
and backend-neutral execution authorities, but must not schedule dependency
graphs or construct a Linux backend.

The preparation layer may depend on the published state-plan, source-plan,
build-plan, planner, image, and application values. It may inspect exact artifact
bytes through an injected backend, but must not observe target paths itself,
normalize package policy, discover runtime closure, execute lifecycle programs,
mutate the target, or publish state.

The check layer may depend on transaction progression, construction evidence,
`libpkgcheck`, and `libpkgcheck-exec`. Pure request admission must remain usable
before host paths exist. Concrete resources, interpreter identity, credentials,
and limits belong only to the admitted check session. The controller must not
reimplement input-tree matching, resource-layout construction, process-status
classification, or backend execution.

The pure dispatch layer may depend on transaction progression and exact admitted
construction, check, and effect sessions. It owns deterministic reservation,
bounded in-flight capacity, caller attempt nonces, and immutable ownership
records. It must not execute a driver, create a backend, allocate resource paths,
construct new transaction edges, or infer success from reservation state.
Check-scoped package inputs require `libpkgtransaction >= 2.1.0` so exact input
authority precedes the construction that seals it.

The single-dispatch execution layer may compose the pure dispatch functions with
explicit run/effect stores and an injected driver. It must commit started
ownership before driver invocation and commit only evidence accepted by the
existing completion functions. Start failure invokes no driver. Driver or final
commit failure leaves started ownership durable. This layer must not reserve
work, loop, discover resources, create backends, choose retries, or release work
automatically.

The run-authority rehydration layer may ask injected caller-owned sources for
one complete semantic progression, one fresh admitted execution authority, or
one exact restart-recovery authority. It must validate the durable record and
selected dispatch before calling a fresh source, delegate subordinate admission
to existing pure start transitions, and require restart evidence to match the
exact retained attempt. It must not load or append journals, execute or
reconcile work, discover paths or evidence, construct a fact from an identity,
choose retries, reserve work, or loop.

The one-step advancement layer may load one committed run head selected by an
exact journal identity, rehydrate its progression through a caller source, and
compose the existing reservation, authority, execution, and reconciliation
functions. Existing active ownership must be reconciled before fresh work is
considered, and the call must return after that attempt. A fresh reservation may
be committed only for a quiescent reopened run and only after the selected
execution dependencies are validated. Execution authority must be acquired
after the reservation commit. The layer must not loop, schedule, create workers,
choose retries or backoff, discover resources or evidence, adopt processes,
roll back, clean up, compact journals, collect history, or expose a mutating
command.

The operation-driver source layer must acquire one call-scoped physical driver
only from an exact validated execution or recovery handoff. Fresh acquisition
must occur after the reservation successor is committed and before effect
admission. The returned driver must retain a live target mutation lease and the
exact state projection sealed into the operation session. Recovery must request
a driver only when the classified effect checkpoint can touch the target or a
successful terminal result requires a resulting-state read. The controller must
not retain or serialize drivers, reuse one driver across dispatches, discover
leases, stores, archives, credentials, or backend paths, or allow source failure
to fabricate effect-attempt ownership.

A concrete native effect-driver source may compose only caller-selected
mechanisms. It may duplicate an already-open lock-directory descriptor, validate
one caller-returned replayable archive, acquire the POSIX outer lease, and derive
application state through the lease-bound canonical-state adapter. It must not
discover paths, stores, archives, credentials, backends, or policy from durable
identities. Incoming archive validation must precede target locking; removal
must not query archive authority. Shared continuation and observation objects
must retain the same lease until both are destroyed. Recovery must acquire only
the authority shape selected by the durable effect classifier.

The durable effect-attempt inspection layer may load one committed head
selected by an exact attempt identity, validate the storage-returned attempt,
classify the controller-owned record through `assess_effect_restart()`, and
render only durable controller evidence. It may expose retained subordinate
identities but must not rehydrate lifecycle results, application receipts or
journals, transaction evidence, publication values, or installed-state
snapshots. It must not enumerate attempts, traverse run journals, construct a
restart checkpoint, append, reconcile, invoke a driver, repair storage, or
expose an effect-implying command.

Read-only POSIX effect-store loading must mirror run-store loading: open an
existing lock with `O_RDONLY`, acquire `LOCK_SH`, and create nothing when the
lock is absent. An unlocked observation must be repeated under the lock if a
writer establishes it concurrently, including after an initially failing read.
Only append may use `O_RDWR | O_CREAT` and `LOCK_EX`.

The durable transaction-run inspection layer may load one committed head
selected by an exact journal identity, validate the storage-returned journal,
classify controller-owned retained ownership, and render a deterministic report.
It may share pure record assessment with restart checkpoints, but it must not
rehydrate semantic progression, infer package facts from identities, append a
record, inspect an effect journal, reserve or execute work, scan directories,
discover journals, or expose a mutating command.

The exact run-inspection command may parse one explicit existing store path and
one lowercase SHA-256 journal identity, open the POSIX run store, delegate to
`inspect_transaction_run()`, and render the existing report. It must not scan or
select journals, reconstruct progression, inspect effect stores, append, reserve,
execute, reconcile, or repair work. Read-only POSIX loads must not create the
writer lock or require a writable store.

The exact effect-inspection command may parse one explicit existing store path
and one lowercase SHA-256 attempt identity, open the POSIX effect store,
delegate to `inspect_effect_attempt()`, and render the existing report. It must
not scan or select attempts, derive an attempt from a run journal, rehydrate
subordinate evidence, construct restart authority, append, reconcile, invoke a
driver, repair storage, or mutate the target. Typed effect-journal diagnostics
must remain distinct. Read-only POSIX loads must not create the writer lock or
require a writable store.

The restart-safe transaction-launch layer may derive one exact journal from an
immutable initial run and caller-owned replay-safe run nonce, load only that
journal's committed head, append sequence zero only when no head exists, and
then delegate to the bounded serial drive. An existing head must match the exact
transaction, nonce, and dispatch-policy admission universe. Failure before the
admission append must perform no drive action; failure afterward must leave the
durable head resumable. Exact retries after successor records exist must resume
the current head rather than republish sequence zero. The layer must not scan
for journals, run without an explicit bound, create workers or concurrency,
choose retry timing or adaptive priority, discover resources or evidence, roll
back, clean up, compact history, collect garbage, or expose a mutating command.

The durable transaction-run admission layer may construct one immutable initial
run from exact progression and dispatch policy, obtain one nonce from a
caller-owned replay-safe source keyed to that run, append sequence zero, and
return only the authority supplied and validated by storage. Exact retries must
reuse the same nonce for the same initial run. Source refusal must precede every
store write, and store failure must not escape as admitted authority. The layer
must not reserve or execute work, load or advance another head, access effect
storage, discover resources, create backends, schedule, loop, choose retry
timing, roll back, clean up, compact history, collect garbage, or expose a
mutating command.

The bounded serial transaction-drive layer may repeat the one-step advancement
function only under an explicit positive caller bound. It must reload the
committed head on every iteration, reconcile retained ownership before fresh
work, and obtain dispatch nonces from a caller-owned replay-safe source keyed to
the exact storage-derived head. The source must not be called for recovery,
stopped or quiescent runs, or after any stopping outcome. Continued steps must
remain in one journal and strictly advance durable head sequence. The layer must
not create workers, concurrency, adaptive priority, unbounded execution, retry
or backoff timing, discovery, process adoption, rollback, cleanup, compaction,
garbage collection, or a mutating command.

The restart-reconciliation layer may consume one exact
`transaction_run_restart_checkpoint`, one retained dispatch, and explicit
caller-rehydrated subordinate authority. Reserved release must require the
`release_reserved` disposition. Construction and check recovery must validate
the exact retained attempt session before storage. Operation recovery must bind
the run-retained effect attempt to the latest supplied effect-journal record and
delegate continuation to `resume_effectful_operation()`. External-resolution
stages invoke no driver and append no run successor. This layer must not scan for
evidence, construct a result from an identity, discover resources or journals,
adopt processes, choose retry timing, reserve work, or loop.

The durable run-journal layer may serialize only controller-owned dispatch
ownership, exact identities, causal sequence, and terminal flags. Reopening must
consume an exact rehydrated `transaction_progress` and revalidate graph units,
predecessor evidence, completed evidence, and active operation state. It must
not deserialize an identity into subordinate semantic evidence. A started
transition must be appended and synchronized before its driver is invoked;
append failure is a stop condition, not permission to execute without durable
ownership. Operation start is stricter: commit the exact effect-attempt
admission first, then the started run snapshot retaining that attempt identity.
The ordered pair must remain exactly retryable, and an orphan admission must
never be interpreted as started target mutation. POSIX changes must preserve
the two-stage commit: immutable record
publication and synchronization first, checksummed head replacement and final
directory synchronization second. Exact retries must be idempotent, while a
foreign same-name record must fail closed. Do not restore operational loading by
scanning every full historical snapshot.

The effectful controller layer may depend on image, plan, apply, execution,
and state adapters only through their exact public values. It must not parse
payloads, derive lifecycle programs, perform target mutation, or serialize
installed state itself. Physical effects stay behind injected authority
interfaces, and the CLI remains separate from the effect-session kernel.

The controller journal is not an alternate application or state store. New
restart paths must consume exact subordinate journals and receipts, never scan
for them, infer them from names, or overwrite ambiguous evidence. A restarted
attempt always proves a newly held physical mutation lease.

## Compatibility policy

Historical `pkgman` compatibility belongs in an explicit translation frontend.
Native command and session semantics are not weakened to preserve ambiguous
legacy behavior.

## Release procedure

1. qualify every commit boundary against exact tagged dependency bundles;
2. run strict GCC and Clang builds and all controller tests;
3. run ASan and UBSan over the complete authority closure;
4. check shared and static linkage and direct dependency isolation;
5. verify construction authority binding, source/build evidence retention,
   check request/session/result binding, canonical multi-input projection,
   concurrency-safe check progression, deterministic dispatch reservation,
   exact predecessor and state-epoch binding, operation-lane serialization,
   failure containment, durable run single-transition sealing, exact progression
   rehydration, graph/evidence revalidation, write-ahead start persistence,
   one-dispatch driver barriers, exact run-authority handoffs, one-step
   recovery-before-reservation ordering, head-derived replay-safe dispatch
   nonce issuance, explicit bounded serial driving, durable
   reservation-before-authority
   acquisition, exact reserved release, caller-rehydrated
   build/check recovery, effect-journal continuation, lost-terminal-write
   recovery, preparation projection and typed refusal, effect sequencing,
   intent-before-effect persistence, exact restart checkpoints, outer-lease
   reacquisition, publication reconciliation, publication provenance, CLI
   read-only behavior, and missing-state refusal;
6. update release metadata and manuals together;
7. compare independently replayed trees and stable patch IDs;
8. tag signed releases only from a clean tree.
