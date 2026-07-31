# pkgctl design

## Purpose

`pkgctl` is the Zeppe-Lin controller. It selects which authoritative component
to call next, retains the exact handoff identities, and reports the resulting
control state. It does not recreate the facts or decisions owned by those
components.

The central invariant is:

> A controller session is evidence that exact authorities were composed; it is
> not another package-source, resolver, transaction, planner, application, or
> state model.

## Release 0.16.0 durable transaction-run admission boundary

Release 0.16.0 gives one transaction-run history an explicit durable origin:

```text
exact transaction progress + dispatch policy
                    |
                    v
          immutable initial run
                    |
                    v
 caller-owned replay-safe run nonce
                    |
                    v
          sequence-zero record
                    |
                    v
             store append
                    |
                    v
       storage-derived reopened run
```

`admit_transaction_run()` first constructs the exact initial `transaction_run`.
Only then may the injected `transaction_run_nonce_source` issue the nonce for
that run identity. Exact retries for the same initial run must issue the same
nonce. The nonce distinguishes durable histories; it does not alter transaction
semantics, dispatch policy, current state, or progression evidence.

The resulting sequence-zero record must contain no predecessor and no dispatch
ownership. The store append is the commit point. The controller validates the
complete authority returned by storage and reopens the run through that record,
so the returned checkpoint is storage-derived rather than a local claim. A
source refusal causes no append. A failed append yields no admitted authority;
an exact retry may converge only through the same nonce and same record.

This layer does not load or advance an existing journal, reserve work, acquire
execution or recovery authority, invoke a driver, access an effect journal, or
compose the bounded drive. It creates no scheduler, worker, concurrency, retry
timing, discovery, rollback, cleanup, compaction, garbage collection, or
command action.

## Release 0.15.0 bounded serial transaction-drive boundary

Release 0.15.0 permits bounded repetition only after nonce issuance is tied to
the committed run head:

```text
load committed head
        |
        +-- active ownership --> reconcile one --> classify
        |
        `-- fresh ready work --> issue nonce(head, run)
                                  |
                                  v
                         reserve and execute one
                                  |
                                  v
                              classify
                                  |
                  stop or reload committed head
```

A caller supplies a positive maximum step count and one replay-safe
`transaction_dispatch_nonce_source`. The source receives the exact
storage-derived journal record and its rehydrated run only when that head has no
active ownership, is not stopped, and exposes canonical ready work. It must
return the same nonce for exact retries against the same head. Once a successor
record commits, that new head is a distinct issuance domain.

This ordering prevents speculative nonce lists. A failure after nonce issuance
but before reservation commitment leaves the old head authoritative, so retry
requests the same nonce. A committed reservation changes the head before any
execution-authority call. Restart therefore reconciles retained ownership
without issuing another nonce; only a later fresh reservation against the
committed release or completion head can request a new one.

`drive_transaction_run()` invokes the one-step controller serially. Each call
reopens storage and either reconciles one retained dispatch or reserves and
executes one fresh dispatch. The drive stops on completion, terminal failure
containment, operation external-resolution authority, incomplete quiescence, or
its explicit step limit. Its result validates one journal, ordered durable
heads, and absence of continuation after a stopping outcome.

The bound is policy supplied by the caller and is not an adaptive scheduler.
The layer creates no worker, concurrency, priority heuristic, unbounded loop,
retry timing, backoff, discovery, process adoption, rollback, cleanup,
compaction, garbage collection, or command action.

## Release 0.14.0 one-step transaction-advancement boundary

Release 0.14.0 composes existing durable authorities into one bounded controller
step without creating a scheduler:

```text
load committed run head
        |
        v
rehydrate exact progression
        |
        +-- active ownership --> reconcile first retained dispatch --> return
        |
        `-- no active work ---> reserve first canonical ready dispatch
                                  |
                                  v
                          commit reservation
                                  |
                                  v
                          acquire exact execution authority
                                  |
                                  v
                          execute behind durable barriers --> return
```

The call accepts a journal identity rather than a caller-provided run snapshot.
The run store therefore supplies the committed head used for all later
validation. `rehydrate_transaction_run()` restores semantic progression through
the caller-owned source before dispatch policy is consulted.

Retained ownership has absolute precedence. The first active dispatch in durable
ledger order is passed through the exact recovery-authority handoff and one
existing durable reconciliation function. Reserved work is released; started
construction and check work require exact result evidence; started operation
work binds the retained effect attempt and its current restart disposition. An
external-resolution disposition returns the unchanged committed head, invokes
no driver, and appends nothing. The call never reserves fresh work after any
reconciliation attempt.

Only a reopened run with no active ownership may call `reserve_next()`. The
selected driver and operation effect store are validated before the reservation
is committed. Fresh execution authority is intentionally acquired only after
that commit. A source refusal therefore leaves a visible reservation, while the
existing write-ahead execution layer leaves visible started ownership after a
driver escape or failed terminal append.

The result constructor reopens the returned run from the storage-derived record
and validates the exact disposition, retained dispatch, and evidence shape. The
step executes or reconciles at most one dispatch and then returns. It creates no
loop, scheduler, worker, concurrency, retry or backoff policy, discovery,
process adoption, rollback, cleanup policy, compaction, garbage collection, or
command action.

## Release 0.13.0 exact run-authority rehydration boundary

Release 0.13.0 separates caller-owned recovery and execution inputs from the
controller actions that consume them:

```text
durable record or restart checkpoint
        |
        v
caller-owned progression, resource, or recovery source
        |
        v
pkgctl exact binding validation
        |
        v
sealed call-scoped handoff
```

`rehydrate_transaction_run()` asks one injected source for the semantic
`transaction_progress` named by a durable run record and delegates reopening to
`transaction_run_restart_checkpoint::make()`. The source owns loading or
reconstruction. The record remains the authority for the exact progression,
transaction, state epoch, policy, dispatch history, and run identity.

`acquire_transaction_dispatch_execution_authority()` accepts one exact reserved
dispatch and asks the caller-owned source for the concrete construction session,
check session, or operation session plus effect-attempt nonce. It invokes the
existing pure `start_*_dispatch()` transition as the validator, but commits
nothing and invokes no driver. Fresh resource choices may differ when their
subordinate admission contracts define them as non-semantic; the sealed handoff
identity still binds the concrete admitted authority supplied for this call.

`acquire_transaction_dispatch_recovery_authority()` starts from one exact
restart checkpoint. A never-started reservation needs no subordinate evidence.
Started construction and check recovery must return a result whose session is
the exact session retained by durable ownership. Started operation recovery
must return the exact effect checkpoint named by both the retained session and
effect-attempt identity. Unlike fresh resources, restart evidence is not
substitutable.

The handoff objects are call-scoped values. They do not append or load journals,
reserve or execute work, reconcile results, discover paths or processes,
serialize subordinate evidence, choose retry timing, loop, schedule, roll back,
compact history, or expose a command. They make those authorities explicit so a
later runner can act without turning identities into facts.

## Release 0.12.0 durable restart-reconciliation boundary

Release 0.12.0 turns one conservative restart classification into at most one
durable reconciliation transition. It is the recovery actuator paired with the
0.11.0 fresh-execution actuator:

```text
exact reopened run + committed record + restart assessment
        |
        v
caller-rehydrated subordinate authority
        |
        v
existing pure release/completion/effect-restart transition
        |
        v
exact committed run successor or explicit external-resolution stop
```

A reserved dispatch may be released only when restart classified it
`release_reserved`; no driver or subordinate result is consulted. Started
construction and check dispatches require exact caller-rehydrated terminal
results. The retained attempt-session identity must equal the result session,
and the ordinary `complete_*_dispatch()` function must accept the result before
the successor can commit. Stored result identities are never promoted into
result objects.

A started operation must retain the exact effect attempt named by the supplied
latest effect restart checkpoint. `assess_effect_restart()` remains the policy
for whether that attempt is automatically continuable. Safe stages continue
through `resume_effectful_operation()`. A terminal effect journal repairs a lost
run completion without a second target mutation. A stage requiring external
resolution returns the unchanged committed run record, invokes no effect driver,
and appends nothing. Successful effect completion is paired with a fresh
authoritative state read before progression advances.

Every successful reconciliation uses `commit_transaction_run_successor()`, so a
failed terminal append leaves the previous committed restart authority intact
and an exact retry converges on the same successor. The API accepts one selected
dispatch and one complete recovery checkpoint. It does not discover evidence,
resources, journals, paths, processes, or leases; choose retries; reserve more
work; loop; schedule; roll back; compact history; or expose an effectful command.

The effect terminal-rehydration path also snapshots transaction evidence before
moving the retained publication request. This makes reconstructed terminal
result identity independent of C++ function-argument evaluation order and
therefore exactly equal to the fresh terminal result.

## Release 0.11.0 single-dispatch execution boundary

Release 0.11.0 turns one caller-selected durable reservation into one driver
invocation. It is an execution kernel, not a scheduler:

```text
selection and resources: caller authority
started ownership:       pkgctl run journal
physical realization:    injected subordinate driver
semantic result:          existing controller evidence
terminal ownership:       pkgctl run journal
```

`commit_transaction_run_successor()` is the common commitment primitive. It
derives the only legal successor from the current journal record, appends it,
and refuses a store that returns another record identity, journal, or sequence.
It does not infer completion and it does not reload semantic evidence.

Construction and check execution follow one write-ahead sequence:

1. validate the supplied reservation and exact admitted session;
2. derive and durably commit the `started` run successor;
3. invoke the injected construction or check driver;
4. submit the returned evidence through `complete_*_dispatch()`;
5. durably commit the resulting run successor.

Failure before step 2 completes grants no execution authority. Failure in steps
3 through 5 leaves the started record committed. The caller must recover exact
construction or check evidence; the controller does not retry a driver or infer
that work did or did not occur.

Operation execution preserves two journals and their distinct meanings:

1. commit the exact effect-attempt admission;
2. commit the started run retaining that attempt identity;
3. execute through the durable effect journal;
4. for completed publication, reread authoritative installed state;
5. submit the effect result and commit one run successor.

An admission without step 2 is audit evidence only and the run remains reserved.
A started run without later effect evidence requires effect-journal inspection.
A terminal effect journal with a lost step-5 run append still leaves the run
started, so restart is conservative and can reconcile from the retained attempt.
Uncertainty results append observations and do not retire ownership.

The API accepts one exact dispatch. It does not call `reserve_next()`, discover
sessions or paths, construct backends, create threads, loop over readiness,
choose retry timing, release work automatically, or expose an effectful command.
Whole-transaction execution remains a later policy layer.

## Release 0.10.0 durable transaction-run boundary

Release 0.10.0 makes the immutable dispatch ledger durable without creating a
second package-state authority. The distinction is strict:

```text
run journal: durable ownership and causal ledger transitions
progression: exact semantic package, check, effect, and state evidence
```

A `transaction_run_journal_record` is one complete append-only snapshot of a
run. Sequence zero must be admitted before the first reservation and therefore
contains no dispatch ownership and every positive sequence contains at least
one retained reservation. Sequence is exact, not advisory: a reserved dispatch
accounts for one transition, started for two plus its observations,
released-unstarted for two, and completed for three plus its observations.
Every successor has the exact prior record identity and advances by one, and
only one, ledger transition. A successor may append one reservation, move one
reservation to started or released-unstarted, append one operation-uncertainty
observation,
or complete one started dispatch while progression advances. A newly appended
reservation must name the predecessor record's exact progression identity and
canonical state epoch; a self-consistent but detached reservation is not a
legal successor.

The record identity binds the exact transaction session identity, explicit
dispatch policy, caller-issued 32-byte run nonce, sequence, predecessor record,
run and progression identities, canonical state epoch, terminal state flags,
and every immutable dispatch record. The binary codec is bounded, deterministic,
versioned, endian-stable, and redundant: subordinate identities are encoded and
recomputed during decoding.

### Rehydration, not fabrication

The journal deliberately stores identities rather than subordinate semantic
objects. Reopening therefore requires the caller to supply the exact
`transaction_progress` named by the record. The controller revalidates every
dispatch unit against the supplied transaction graph, every predecessor binding
against retained terminal evidence, every completed dispatch against progression
terminal evidence, and every active operation against the current state epoch.
Only then is the immutable `transaction_run` reconstructed.

A stored digest cannot become a `construction_result`, `transaction_check_result`,
`effectful_operation_result`, or `pkgstate::snapshot`. Those values must be
rehydrated from their owning authorities before the run can reopen.

### Write-ahead ownership

Construction and check starts use one run-journal barrier:

```text
admit exact session
        -> seal started run successor
        -> commit and synchronize run successor
        -> invoke admitted driver
```

Operation starts cross the run and effect journals. Their only safe order is:

```text
derive exact effect-attempt admission
        -> commit effect-attempt admission
        -> seal started run successor retaining attempt identity
        -> commit started run successor
        -> invoke effect driver with the same attempt nonce
```

`commit_operation_dispatch_start()` implements the two-store prefix and verifies
that both abstract stores return the exact expected authority. Both appends are
idempotent, so the whole prefix is exactly retryable. If the first append commits
but the second does not, the effect admission is an inert orphan: the committed
run still says `reserved`, restart may release it, and no target mutation is
inferred. If the started run commits but execution has not begun, restart names
the exact effect-attempt journal and continues conservatively from its admitted
record.

No driver may run before the applicable started-run commit. If a run append or
synchronization fails, execution must not begin. This is the controller
write-ahead barrier that makes a recorded `reserved` dispatch safe to release
after restart.

### Conservative restart classification

Restart assessment does not execute or infer outcomes:

- a `reserved` dispatch is classified `release_reserved`;
- a started construction is `recover_construction`;
- a started check is `recover_check`;
- a started operation is `inspect_effect_journal` and retains the exact
  effect-attempt identity to inspect.

Completed and released records are quiescent. Operation uncertainty observations
remain ordered on the active dispatch and are passed to the caller for exact
effect-journal reconciliation. `transaction_run_restart_checkpoint` combines
only an exact reopened run, its durable record, and this conservative assessment.

The supplied POSIX store is rooted at an explicit directory or directory file
descriptor. It serializes writers with a directory-local lock, uses no-follow
opens, writes a private temporary snapshot, seals and synchronizes it, publishes
the immutable record without replacement, synchronizes the directory, and then
atomically publishes a checksummed read-only head checkpoint followed by another
directory synchronization. The head is the physical commit point.

Operational recovery does not infer the newest state by scanning filenames. It
reads the head, opens the one exact self-contained snapshot named by that head,
and verifies journal, sequence, and record identity. A missing head in the
presence of record names, a corrupt head, or a missing or corrupt head-selected
snapshot fails closed. Historical snapshots are inert causal and audit material;
their deletion does not fabricate another committed state. An exact retry after
a crash between record publication and head publication verifies the existing
snapshot and promotes the same authority idempotently. An exact retry after head
publication is also idempotent.

The store is crash-consistent, not an anti-rollback trust anchor. A privileged
actor that can replace the head with an older valid checkpoint is outside this
boundary and requires an external monotonic anchor if rollback detection is a
system requirement. The store owns no ambient package-manager path.

Every run record is a complete snapshot, bounded to 16 MiB. Release 0.10.0 adds
no compaction or garbage collection, so historical write volume is intentionally
not hidden. A delta/checkpoint format may replace this physical representation
before an automatic whole-transaction executor is admitted; semantic record and
run identities must remain authoritative across such a storage migration.

Release 0.9.1 established the same head discipline for effect-attempt encoding
version two. Every selected snapshot is self-validating: its sequence must equal
the exact transition count implied by retained lifecycle, application,
publication, and terminal evidence. A terminal lease-loss snapshot cannot hide
an unresolved publication intent. Version-one record-only histories remain
readable by strict full semantic-chain validation. Appending a new version-two
successor establishes its durable head. An exact retry of the latest legacy
snapshot first atomically rewrites that selected snapshot in version-two form,
synchronizes the directory, and only then publishes a head that can name it.
Release 0.10.0 consumes that established commit point; it does not redefine
effect-journal recovery.

This release adds no journal discovery, semantic-evidence serialization,
automatic release, driver invocation, resource recovery, cleanup, retry,
rollback, or command frontend.

## Release 0.9.0 transaction-dispatch boundary

Release 0.9.0 adds immutable ownership of selected but unfinished work. It does
not add an execution loop. The distinction is deliberate:

```text
dispatch:    who owns this unfinished realization unit?
progression: what terminal facts are now known?
```

A `transaction_run` retains one exact `transaction_progress`, one explicit
`transaction_dispatch_policy`, and the complete lifetime ledger of dispatch
records. Its identity binds all three. Records are never removed: completed and
released attempts remain present so a caller-issued nonce cannot be reused and
historical ownership cannot disappear from the run.

### Deterministic reservation

`reserve_next()` scans the canonical `ready_units()` order and reserves the
first unit allowed by policy. Construction and check capacities are explicit
positive bounds. Operation capacity is an invariant fixed at one. Active graph
members may not overlap, so an operation reservation owns its target action and
all absorbed lifecycle nodes as one indivisible unit.

A 32-byte, nonzero caller nonce identifies the physical dispatch attempt. The
nonce is not randomness authority and does not change transaction semantics; it
prevents two attempts for the same semantic unit from becoming
indistinguishable. A nonce is consumed for the lifetime of the run even when an
unstarted reservation is released.

Reservation captures the current progression identity, canonical state epoch,
and exact terminal evidence for every non-retain predecessor outside the unit.
The dispatch identity binds that evidence. Completion after unrelated progress
is therefore valid only while the same unit remains ready and its exact direct
predecessor evidence remains unchanged.

### Reserved and started work

Dispatch records have four explicit states:

1. `reserved` — ownership exists, but no execution authority has been admitted;
2. `started` — one exact construction, check, or effect session is bound;
3. `completed` — authoritative terminal evidence advanced progression;
4. `released_unstarted` — ownership was returned before execution admission.

Only `reserved` work may be released. A started operation cannot be converted
back into unstarted work merely because the controller has not yet received
terminal evidence. This prevents lost processes, lease loss, or publication
uncertainty from being misreported as harmless cancellation.

Starting a construction dispatch validates every exact package-input edge. A
built input must reproduce the selected package release, source snapshot, build
result, artifact, and retained predecessor construction evidence. A retained
installed input must still name the same installed package in the current state
and reproduce its source and build provenance. `libpkgtransaction >= 2.1.0`
orders both build- and check-scoped package inputs before the construction that
seals them.

Starting a check dispatch requires the exact construction result already
retained by progression and captured by the dispatch dependency. Starting an
operation requires the exact action, transaction, admitted effect session, and
state epoch against which the operation was reserved.

### Completion and uncertainty

Construction and check completion require the exact started session, then
delegate terminal graph truth to `advance_construction()` or `advance_check()`.
The dispatch layer does not reinterpret subordinate outcomes.

Definitive operation results delegate to `advance_effect()`. Lost outer lease
and indeterminate publication are not terminal. Their exact result identities
are retained as ordered observations while the dispatch stays active. A later
authoritative result for the same exact effect session may retire it. An
uncertain observation cannot carry a resulting state snapshot.

### Failure containment

The only 0.9.0 containment mode is
`stop_after_terminal_failure`. Once progression contains terminal failure, no
new unit is reserved and merely reserved work may not start. Already-started
independent work may still report exact terminal evidence. This separates
failure containment from fabrication: work known to have started must still be
accounted for.

### Deliberate boundary

The 0.9.0 dispatch model itself performs no source acquisition, build, check,
lifecycle, application, publication, backend construction, retry, or rollback.
It creates no threads, wait loop, resource paths, durable run journal, or
effectful CLI command. Release 0.11.0 composes that pure model in a separate,
explicit one-dispatch execution layer.
Adaptive priorities, work stealing, critical-path scoring, fairness, and
transaction-wide continuation after failure remain outside this release.

## Release 0.8.0 transaction-check boundary

Release 0.8.0 closes one exact check unit without adding scheduler policy.
The boundary is deliberately split in two.

`transaction_check_request::make()` is pure controller admission. It accepts a
`transaction_progress` and one check-node identity. The node must be exactly
ready, must have one exact `build_before_check` predecessor, and that build node
must already retain successful construction evidence in the same progression.
The resulting request retains the transaction session, preparation-progression
identity, check-node identity, complete construction result, and the sealed
`libpkgcheck` request.

`transaction_check_session::admit()` binds that pure request to concrete
source, built-package, check-input, root-view, private-temporary, interpreter,
credential, and resource-limit coordinates. The exact resources are delegated
to `libpkgcheck-exec >= 0.1.1`. Lower missing, duplicate, forged, aliased, or
path-invalid resource failures are surfaced as controller-owned
`invalid_check_session` refusal. The controller session identity binds the
concrete coordinates and the exact prepared `libpkgexec` request identity.

Execution remains injected. `execute_transaction_check()` asks one
`transaction_check_driver` for terminal evidence and validates that both the
check request and execution request are exact. A native driver composes
`libpkgcheck-exec` with an injected `libpkgexec` backend. Driver exceptions,
foreign requests, and lower backend-contract failures are controller driver
contract violations; they are never promoted into transaction progress.

`advance_check()` accepts passed or failed terminal evidence only when the
transaction, program, check node, check request, execution request, and retained
construction authority all remain exact. Passed evidence satisfies the check
node. Failed evidence fails it and blocks graph successors by ordinary
progression derivation.

Completion admission intentionally does not require equality with the whole
progress identity captured during preparation. Independent ready units may
advance concurrently. A prepared check remains admissible when its exact node
is still ready and the exact construction evidence on which it depends remains
retained. This is concurrency without stale-dependency amnesty.

The release adds no ready-peer selection, retry policy, backend construction,
automatic execution, transaction-wide cancellation, durable check-session
store, or effectful CLI command.

## Release 0.7.1 transaction-progression boundary

Release 0.7.1 makes controller knowledge about one immutable transaction
program explicit. A `transaction_progress` value retains:

1. the exact sealed `transaction_session`;
2. the current canonical `libpkgstate` snapshot epoch;
3. accepted terminal `construction_result` values by exact build node;
4. accepted terminal `transaction_check_result` values by exact check node;
5. accepted terminal `effectful_operation_result` values by exact target action;
6. derived status for every transaction node;
7. every graph-ready realization unit, without selecting one.

Progression starts from the installed snapshot retained by the transaction's
resolution. Exact `retain` nodes are initially satisfied. All other status is
derived from graph predecessors and retained terminal evidence; the controller
does not reinterpret resolver reasons or compose new transaction edges.

### Realization units

A raw transaction node is not always a physically selectable unit. Every
`build` node is one construction unit and every `check` node is one check unit.
An `install`, `upgrade`, or `remove` node absorbs the lifecycle nodes connected
to it by exact `pre_lifecycle_before_action` and
`action_before_post_lifecycle` phase edges. The resulting operation unit is
ready only when every predecessor outside that member set is satisfied.

This unit boundary prevents the controller from waiting for an internal
pre-lifecycle node before selecting the operation that must execute it. It does
not erase node-level evidence. A successful effect satisfies all members. After
a definitive failed effect, executed lifecycle nodes retain their individual
success or failure, the target action is failed, and unexecuted lifecycle
members are blocked.

Independent ready units remain simultaneously visible in canonical primary-node
order. Progression supplies no priority, tie-break, parallelism, fairness,
resource, retry, or failure-containment policy.

### Evidence admission

`advance_construction()` accepts evidence only when:

- the construction belongs to the same transaction session;
- its exact build node is currently ready;
- that node has no earlier terminal construction evidence.

A successful result satisfies the build node; a failed result fails it and
blocks dependent units through normal graph derivation.

`advance_effect()` additionally requires the effect request's expected state to
be the exact current progression epoch. A completed effect must carry the exact
state-publication request and either a successful `libpkgstate` publication
receipt proving the caller-supplied resulting snapshot or authoritative restart
reconciliation naming that snapshot. Only then does progression replace its
current state epoch.

Definitive lifecycle, application, or publication failure terminates the
operation without advancing state. Lost-lease and indeterminate-publication
outcomes are not terminal progression evidence because authoritative state is
not yet known.

### Current-state preparation

Operation preparation now accepts `transaction_progress`, not a bare
transaction session. The selected target action must be ready, and incoming
construction must already be retained by that progression. Planner state facts
are projected from `current_state()`.

Installation requires the package still to be absent. Upgrade and removal
require the exact installed-package identity captured by the original
transaction action to remain present unchanged in the current epoch. This keeps
the original transaction authoritative while refusing silent re-resolution
after earlier effects.

Every effect request retains its exact expected state snapshot. Application
preconditions, target binding, publication projection, durable restart, and
later progression admission therefore share one explicit state epoch rather
than falling back to the transaction's original snapshot.

### Progression identity

Progression identity binds the transaction session, current state snapshot,
ordered terminal construction, check, and effect identities, every derived node status,
and the ready-unit identities derived from that authority. Host paths, execution
timing, and caller choice among ready units remain outside the identity.

### Deliberate boundary

Progression is pure controller-state derivation. It does not materialize
sources, execute builds or checks, prepare check resources or operations,
execute lifecycle or application effects, publish state, resume journals, or
call a platform
backend. Check units may become ready but there is deliberately no
`advance_check()` API: no supplied authority currently defines a sealed check
request and terminal check result.

Release 0.8.0 adds no durable progression/check store or effectful command frontend.

## Release 0.6.0 operation-preparation boundary

Release 0.6.0 prepares one exact target action already present in a sealed
transaction program. It accepts only `install`, `upgrade`, or `remove`. The
caller supplies:

1. the exact `transaction_session` and target-action node;
2. one matching completed `construction_result` for install or upgrade;
3. one caller-authoritative application target and execution-control snapshot;
4. complete target observations and normalized package policy;
5. one resolved runtime-closure identity for incoming operations;
6. one complete lifecycle order and, for installation, an installation reason;
7. one injected read-only artifact-projection driver.

The controller first projects the complete canonical installed snapshot through
`libpkgstate-plan`. Incoming operations must identify the exact transaction
`build` node ordered before the target action, and both nodes must carry one
resolver selection. `libpkgbuild-plan` then reopens the retained artifact,
verifies its bytes and payload against the build result, and projects the exact
candidate, image, and artifact facts required by `libpkgplan`.

Preparation requires the new inspection to reproduce the construction receipt's
archive digest, normalized image identity, and entry count. The inspection
backend identity may differ: it is evidence provenance, not package truth. The
result identity nevertheless retains the exact new receipt and incoming-package
authority actually used for planning.

The package-local sequence is:

```text
validate transaction action and construction binding
        ↓
project complete installed truth through libpkgstate-plan
        ↓
reinspect and project incoming artifact through libpkgbuild-plan
        ↓
admit incoming package authority through libpkgapply
        ↓
call the exact libpkgplan install, upgrade, or removal planner
        ↓
retain typed refusal, or seal libpkgapply and pkgctl effect requests
```

Removal has no incoming side and never calls the artifact driver. For upgrade,
the old installed authority is taken from the exact transaction snapshot; it is
not reconstructed from current candidate control.

The effect request retains the complete transaction session and selects one exact
target action from it. Release 0.6.0 therefore removes the initial 0.3 admission
restriction to single-package, single-mutation, acyclic programs. This does not
choose a schedule: all unrelated nodes, other target mutations, and runtime
cohorts remain inert authority until an external scheduler selects another exact
action.

### Preparation refusal

A `libpkgplan` refusal is a complete terminal preparation result. `pkgctl`
retains its operation-specific refusal identity and code together with every
projection already established, but creates no package plan, application
request, or effect request. Adapter exceptions and controller cross-binding
violations remain errors rather than synthetic planning refusals.

### Preparation identity

Preparation-request identity binds the transaction session, exact action node,
completed construction where required, target application context, execution
control, observations, runtime closure, normalized policy, lifecycle order, and
installation reason. Preparation-result identity adds the projected installed
ownership universe, exact build/candidate/artifact/image/inspection evidence,
incoming application authority, and either the planner refusal or the complete
plan, application, and effect identities.

### Read-only boundary

Preparation performs no target mutation. Incoming preparation may read exact
artifact bytes through an injected `libpkgimage` backend, but it does not acquire
the target lease, admit executable lifecycle sessions, call `libpkgapply`, or
publish installed state. It adds no scheduler, recursive construction, check
execution, durable preparation journal, or command frontend.

## Release 0.5.0 construction boundary

Release 0.5.0 adds one backend-neutral construction session for one exact
catalog-backed `build` node already present in a sealed transaction program.
The caller supplies:

1. the exact `transaction_session` and build-node identity;
2. every exact `libpkgbuild` package-input subject and tree identity;
3. resolver-selected build and target architectures retained by the node;
4. one closed build policy and bounded source-acquisition policy;
5. explicit local-source, content-store, root-view, workspace, output, and
   artifact coordinates;
6. concrete dependency-tree paths, interpreter identity, and numeric
   credentials;
7. one injected backend-neutral construction driver.

Admission proves that the selected node is a catalog-authorized build node, its
candidate source and release agree with resolver authority, and all build/check
inputs form a valid `libpkgbuild` input set. Each input must match the resolver's
exact required package, release, source snapshot, and build environment. Every
concrete dependency tree belongs to one exact requested input. Call-scoped paths
are normalized and checked for unsafe overlap before source acquisition begins.

The sequence is:

```text
validate construction authority and resources
        ↓
materialize the exact source snapshot through libpkgfetch
        ↓
translate observed digests into the complete libpkgbuild source set
        ↓
seal the exact build request using resolver architectures and caller policy
        ↓
admit and execute one libpkgbuild-exec session
        ↓
retain verified materialization, execution evidence, build result,
artifact binding, and independent image-inspection receipt
```

The controller promotes `completed` only when the build result succeeded and
independent artifact-inspection evidence is present. A backend or adapter
failure remains a failed build result. Materialization exceptions and driver
contract violations remain explicit failures; they are not converted into
synthetic success or partial authority.

### Construction identity

Construction-request identity binds the transaction-session identity, exact
build node, source snapshot, canonical build-input set, selected architectures,
build policy, and acquisition bounds. Construction-session identity adds the
logical root-view identity, interpreter, canonical credentials, and artifact
compression.

Local source roots, content-store roots, dependency-tree paths, workspace and
output paths, artifact destinations, cache hits, redirects, and transfer timing
remain operational coordinates or evidence. Equivalent sessions at different
host paths therefore retain one semantic identity.

### Deliberate exclusions

The 0.5 boundary does not recursively schedule requirements, choose a build
order, create package-input trees, execute check nodes, construct package-local
application plans, publish installed state, or expose construction commands.
It does not depend on `libpkgexec-linux`; a caller injects the execution backend.

Construction is not added to the durable target-effect journal. A build intent
without terminal evidence is not treated as completed or automatically replayed.
Durable construction attempts require a later explicit controller contract.

## Release 0.4.0 durable attempt boundary

Release 0.4.0 makes the outer controller sequence durably observable. One
caller-issued `effect_attempt_nonce` distinguishes a physical controller attempt
without changing the semantic effect-session identity. Admission creates one
identified attempt, and every later record is a complete immutable snapshot
binding:

1. the exact effect-session identity and nonce;
2. a monotonic sequence number and predecessor-record identity;
3. the durable stage and any active lifecycle index;
4. completed pre-lifecycle result identities;
5. application receipt, application-journal, and completed-evidence identities;
6. completed post-lifecycle result identities;
7. transaction evidence and the exact state-publication request;
8. publication receipt and resulting-state evidence;
9. the terminal controller outcome and any reconciled state identity.

The controller appends an intent snapshot before every irreversible authority
call and a terminal-evidence snapshot after that call returns. The durable
sequence is therefore:

```text
admitted
  -> pre intent / pre terminal ...
  -> application intent / application terminal
  -> post intent / post terminal ...
  -> publication intent / publication terminal
  -> controller terminal
```

A checksummed record is not accepted merely because it decodes. The model also
validates causal shape: application cannot precede the complete pre phase,
post lifecycle cannot precede completed application evidence, publication
cannot precede the complete post phase, and terminal outcomes must carry the
subordinate evidence required by that outcome.

### Storage boundary

`effect_journal_store` is an injected controller authority. The supplied POSIX
implementation is anchored to an explicit directory pathname or already-open
directory descriptor. It publishes immutable, read-only snapshots without
replacement, synchronizes record and directory durability, validates
filename/content identity agreement, rejects corrupt or conflicting chains,
and never searches ambient package-manager state.

The controller journal does not replace the `libpkgapply` application journal.
It records the application handoff and returned receipt; `libpkgapply` remains
the sole authority for package-filesystem recovery.

### Restart classification

`assess_effect_restart()` is pure classification over one validated latest
snapshot. Automatic continuation is conservative:

- admission or completed pre-lifecycle evidence may continue normally;
- lifecycle intent without terminal execution evidence requires external
  resolution because lifecycle programs may be non-idempotent;
- application intent requires the caller to supply the exact durable
  `libpkgapply::application_journal_record` belonging to the request;
- completed application evidence permits continuation into post lifecycle;
- publication intent requires authoritative installed-state rereading;
- an exact prior state permits retry of the retained publication request;
- an exact resulting state permits terminal reconciliation without a second
  publication;
- contradictory state, missing subordinate authority, or mismatched evidence
  requires external resolution.

`pkgctl` does not scan for an application journal, infer it from filenames, or
claim that the lease held before a crash survived. `resume_effectful_operation()`
requires a newly held physical target-mutation lease and revalidates every
supplied subordinate result against the durable controller record.

A publication receipt already retained as indeterminate remains evidence. If
rereading state cannot prove its exact resulting state, the controller does not
discard that receipt and retry as though the first publication never happened.

### Identity compatibility

The semantic effect-session identity remains unchanged. Live control state,
restart timing, physical paths, and store coordinates stay outside that
identity. Ordinary 0.3-style results retain their existing identity domain;
only a result sealed through installed-state reconciliation binds the additional
reconciled-state identity.

## Release 0.3.0 effect boundary

Release 0.3.0 adds one effectful library session for one exact target operation.
The caller supplies:

1. one sealed `transaction_session`;
2. one exact install, upgrade, or removal node from that transaction program;
3. one matching `libpkgapply` application request;
4. the exact pre- and post-action lifecycle-node order;
5. admitted `libpkgapply-exec` sessions for those lifecycle nodes;
6. one outer `libpkgapply` target-mutation lease and its state projection;
7. physical application, lifecycle-execution, and state-publication authorities.

The controller validates that all values belong to one transaction, installed
state, package, target, and application authority universe before effects begin.
The initial boundary rejects multi-package programs, runtime cohorts, and more
than one target mutation.

The sequence is:

```text
validate exact authority universe and outer lease
        ↓
execute every selected pre-action lifecycle node
        ↓
realize the exact libpkgapply request
        ↓
execute every selected post-action lifecycle node
        ↓
seal transaction evidence from all completed subordinate evidence
        ↓
project and publish libpkgstate with that transaction evidence
```

No state publication request is constructed until every required lifecycle node
and the filesystem application have completed successfully. The same
transaction-evidence identity is retained by the publication request and, for
install or upgrade, the durable package receipt.

### Lifecycle order

`libpkgtransaction` determines which lifecycle nodes must occur before and after
the selected action. Where multiple nodes on the same side have no graph edge
between them, the graph deliberately does not choose their order. The caller
supplies one exact order; `pkgctl` validates that it contains the complete phase
set and binds the order into request identity.

For upgrade, this permits one explicit order over both historical installed
removal actions and incoming installation actions around the single upgrade
node. The controller does not invent a synthetic remove action.

### Outer lease

One target-mutation lease must remain held from the first lifecycle effect
through state publication. The controller checks the lease before every stage
and again after publication. A lost lease is retained as a terminal controller
outcome, including any subordinate evidence already produced. The controller
does not silently promote a publication completed after lease loss.

### Failure knowledge

The effect result distinguishes:

- lifecycle failure before application;
- application not completed;
- lifecycle failure after application;
- outer lease loss;
- state publication not completed;
- state publication indeterminate;
- completed publication.

These are controller observations, not rollback claims. A post-action lifecycle
failure can occur after the package filesystem transition completed. An
indeterminate publication requires authoritative rereading of installed state.
Arbitrary lifecycle side effects are never claimed reversible.

## Release 0.2.0 read-only boundary

The read-only pipeline remains:

```text
catalog_request
        ↓
libpkgcatalog-acquire::acquire_catalog
        ↓
catalog_session
        ↓
libpkgstate::canonical_generation_store::open_existing + read
        ↓
libpkgresolve::resolve
        ↓
resolution_session
        ↓
libpkgtransaction::compose
        ↓
transaction_session
```

The command frontend never initializes a missing state store. The state
pathname and all five target-binding identities are supplied explicitly. A
binding mismatch, missing store, corrupt generation, or locked store is a
state-authority failure.

## Owned semantics

The controller owns only:

1. explicit command input and defaults;
2. collection argument order as acquisition precedence;
3. explicit state location and target binding;
4. explicit build and target architectures;
5. typed resolution goals;
6. installed-versus-catalog preference;
7. opt-in transaction convergence policy;
8. authority-call sequencing;
9. controller request, session, result, and transaction-evidence identities;
10. explicit lifecycle ordering where the transaction graph leaves peers
    unordered;
11. outer-lease observation around the composed effect sequence;
12. durable controller-attempt sequencing and restart classification;
13. exact one-node source/build construction sequencing;
14. package-local projection, planning, and request-sealing order;
15. deterministic line-oriented presentation.

Package and profile identities are `libpkgsource` values. Catalog candidates are
`libpkgcatalog` values. Installed records are `libpkgstate` values. Selections
and witnesses are `libpkgresolve` values. Transaction nodes and edges are
`libpkgtransaction` values. Package-local plans and filesystem evidence are
`libpkgplan` and `libpkgapply` values. Lifecycle nodes and execution evidence are
`libpkgapply-exec` and `libpkgexec` values.

## Requests and sessions

A `catalog_request` contains explicit acquisition specifications and document
limits. It does not infer roots, collection names, revisions, or precedence.

A `resolution_request` adds one existing canonical state location, architecture
context, typed goals, and resolver policy. Duplicate semantic goals are rejected
even when their diagnostic origins differ.

A `transaction_request` adds one native convergence policy. The default is
`preserve_unselected`. `converge_exact` means the caller supplied the complete
desired target closure and must be explicit.

A catalog session retains the acquisition request and sealed catalog snapshot.
A resolution session retains that catalog session, one installed-state snapshot,
and one resolution result. A transaction session retains the resolution session
and one transaction program.

A `construction_request` retains one exact transaction build node, canonical
package-input facts, selected architectures, build policy, and source-acquisition
bounds. A `construction_session` adds explicit source/store and build coordinates,
concrete package-input trees, interpreter, credentials, and compression. Host
paths remain outside semantic identity.

A `transaction_progress` retains the transaction session, current canonical
state epoch, accepted terminal construction and effect evidence, exact node
status, and every graph-ready realization unit.

An `operation_preparation_request` retains that progression, one exact ready
target action, its completed construction where required, caller target
observations and policy, target application authority, execution control,
runtime closure, lifecycle order, and installation reason. Its result retains
either one official planner refusal or the exact plan, application request, and
effect request.

An `effectful_operation_request` retains the transaction session, exact expected
state epoch, selected action node, exact application request, caller-selected
lifecycle order, and an installation reason only for installation. An
`effectful_operation_session` adds admitted lifecycle execution resources in
the requested order. A durable
effect attempt adds a caller nonce and append-only controller snapshots; it does
not alter the semantic session identity.

Session identities use domain-separated SHA-256. Diagnostic revisions,
command-line positions, and host filesystem paths remain outside semantic
identity. Effect-session identity binds the lifecycle nodes, logical root-view
identities, interpreter, and numeric credentials; host paths are call-scoped
materialization coordinates.

## Presentation and CLI boundary

Reports remain deterministic line-oriented diagnostics. They expose exact
session and subordinate authority identities but are not authority themselves.
A machine protocol requires a separate versioned contract.

Release 0.8.0 adds no effect-implying CLI command. `catalog`, `resolve`, and
`transaction` remain read-only. Recursive construction scheduling, check
execution, selecting among ready units, durable progression or preparation,
recovering incomplete application
attempts, and exposing install/update/remove policy remain later controller
work.

## Non-authorities

`pkgctl` does not:

- parse Pkgfile or the historical package database;
- infer package identity from directory or archive names;
- select dependency candidates itself;
- compose transaction nodes or edges itself;
- derive package-local filesystem consequences;
- derive or reinterpret lifecycle programs;
- reinterpret source digests, build payloads, or archive inspection semantics;
- execute through a Linux-specific backend directly;
- mutate package files outside `libpkgapply`;
- publish installed state outside `libpkgstate`;
- claim transaction-wide atomicity or rollback;
- choose among graph-ready units or execute a cross-package schedule;
- discover, scan for, or reconstruct a `libpkgapply` application journal;
- infer success for a lifecycle intent lacking terminal execution evidence.

## Clean-room rule

The project remains original Zeppe-Lin code. Public legacy commands and observed
behavior may inform a future migration frontend, but native controller types and
control flow do not inherit `pkgman`, CRUX `prt-get`, or `pkgmk` internals.

Compatibility translation must remain outside the native request and session
model.
