# pkgctl

Release 0.39.0 closes phase-local construction/check authority around the
current native build frontend. The sealed build request still retains both
build- and check-scoped logical requirements, but construction concretizes only
build inputs. Check source, checked-package, and candidate check-input resources
are realized independently from retained source and exact artifact/image authority;
installed check inputs come from state-owned retained resources. CHECK does not
rediscover or re-resolve why those resources belong to the transaction, and no existing execution
tree is accepted as truth merely because bytes are present. Complete build
policy is admitted and retained as one start-only command authority, and terminal
private construction/check realizations are disposed only from completed durable
dispatch authority. The release also requires libpkgexec-linux 0.7.1 so native
preflight receives repeatable descriptor-based isolated realization with private
mount-propagation sealing.

Release 0.38.0 makes native construction and check restartable across
process death at their durable execution boundaries. Exact admitted attempt
authority is retained before `started`; absent terminal evidence is replayed only
from that authority, while retained terminal evidence can retire a run without
reacquiring execution capability. Construction artifacts are sealed and verified
privately first, terminal construction evidence becomes durable second, and the
exact retained bytes are projected into the caller's public artifact root last.
Recovery therefore neither reconstructs historical success from filesystem
residue nor requires a second build to reproduce identical archive bytes.
Privileged qualification kills the real `pkgctl build` process at construction
`started`, check `started`, and artifact publication boundaries and requires exact
resume to complete.

Release 0.37.0 adds `pkgctl build PACKAGE`, the first package-artifact
frontend. It is a constrained caller of the same sealed transaction and durable
native run kernel as `pkgctl run`: the exact package becomes a build goal,
`--check` adds its check goal, catalog authority is preferred, and the composed
transaction must contain a catalog-backed build node for that exact package (plus
its check node when requested). Installed authority therefore cannot silently
satisfy a build request without construction. The frontend carries no lifecycle
root, managed target root, lifecycle credentials,
state-publication backend, target lock, or convergence policy.

The frontend admits recipe syntax through `libpkgsource-yaml` 1.1.x and requires
`libpkgsource` ABI 4 so explicit `unpack: archive` source realization reaches the
sealed source/build authority rather than being reconstructed from filenames.

The final immutable package archives are published beneath one explicit existing
`--artifact-root`, separate from the private runtime hierarchy. Frontend kind and
artifact-root coordinate are retained in command evidence; resume refuses either
a frontend mismatch or artifact-root redirection before durable advancement.
Build output reports the exact retained artifact path and release/artifact/build
identities, digest, size, and image/binding identities as historical admitted
authority; it does not reopen a completed archive merely to claim present truth.
Ordinary `pkgctl run` output reports the same successful construction
evidence when its transaction builds packages, so a product controller can bind
later composition to exact retained construction results. Those run paths remain
beneath the run-private runtime hierarchy; only the `build` frontend projects
artifacts into caller-selected public `--artifact-root` authority.
Privileged qualification
drives a real shell/source/dependency/check campaign across bounded start/resume,
then removes the live collection and proves terminal replay returns the same
artifact inventory with zero durable work and unchanged canonical target state.

A separate privileged empty-target campaign drives a profile with distinct
build-only and runtime dependencies into a fresh canonical state/target pair.
After convergence, a test-only `libpkgaudit` oracle independently observes the
target against canonical ownership facts, requires a complete zero-finding
report, then proves one deliberate owned-object deletion is reported as drift.
This qualifies rootfs composition as ordinary desired-state convergence plus an
independent feedback path; it does not add a rootfs verb to `pkgctl`.

Release 0.36.0 removes target-operation authority from native
construction/check-only runs. Such a sealed transaction enters the same durable
run kernel without opening lifecycle or target roots, target-lock authority,
application storage, operation restart bodies, or canonical-state publication
authority. Supplying those mechanisms to the reduced native composition is a
configuration error rather than harmless surplus capability. The bounded CLI
selects this reduced composition after sealing the transaction; privileged
qualification builds one real package archive while the target-operation
namespaces remain absent and canonical target state remains unchanged.
A sibling privileged native-construction campaign crosses the process boundary
with an actual POSIX shell: local sources are materialized through `libpkgfetch`,
a predecessor package is constructed and mounted as the dependent build input,
the dependent recipe consumes both authorities, and its real check executes
against the staged source and constructed package tree before both archives are
verified. The synthetic static interpreter remains only the smaller isolation and
restart fixture; it is not the construction-reality proof.

Release 0.35.1 hardens retained private journal and evidence reopening so
corrupted special-file authority fails closed without blocking recovery.

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.39.0 also adds terminal disposal of private construction/check
realizations. A successful durable run head, not directory discovery, authorizes
the exact construction-session, package-output, check-resource, and check-temporary
leaves owned by completed dispatches. Released reservations own no disposable
execution tree. Incomplete and failed runs retain their realizations for
restart or diagnosis. Cleanup is idempotent operational work rather than new
historical evidence: a refused or interrupted sweep leaves the completed
transaction intact and a terminal resume retries it. Descriptor-anchored no-follow
removal refuses hostile path substitution. Because realization owners may seal private
directories read-only, cleanup may add owner removability only through already-opened
authorized private directory descriptors; caller-owned runtime class roots are never
chmodded. Public artifacts, content, run and execution evidence, command authority,
collection projection, and canonical state remain outside the cleanup boundary.
Privileged qualification exercises partial build/check stages, process death after
terminal commit, sealed-directory and symlink assault, and zero-work cleanup retry.

Release 0.35.0 closes the functional package-management chain with one
bounded native transaction command. Before fresh-run retention or admission,
`pkgctl run` proves that the selected native Linux backend can establish the
execution guarantees implied by the transaction. `--start` then retains the
complete start-only transaction inputs, one exact admitted `libpkgbuild`
build-policy value, one complete controller-owned native target-operation policy
profile for the run frontend, the exact admitted construction/check/lifecycle
backend capability profiles, and the owner-encoded catalog/state universe before
admitting one explicit run nonce. The target-operation policy is retained only
through its pkgctl owner encoding; operation-session evidence does not carry a
second `libpkgplan` policy codec. `--resume` requires that retained
command evidence and the exact admitted journal; it refuses a second collection,
target-binding, architecture, goal, resolution-policy, or convergence request.
The current canonical-store pathname remains live resume authority while its
target binding comes from retained state evidence. Both enter only through
`native_posix_transaction_run_runtime` and perform at most the explicit
`--max-steps` bound.

The command retains subordinate effect bodies in owner encodings before the
controller journal may reference them. Construction and check now follow the
same write-ahead rule: the exact controller-owned admitted attempt session is
immutable evidence before the `started` run successor can become durable.
Terminal fetch/build or check evidence remains a separate later record. Restart
prefers that terminal evidence; when process death leaves only durable started
ownership, it decodes and replays the exact retained attempt session instead of
reconsulting current construction/check configuration. Successful construction
seals its verified archive beneath the private attempt session; terminal build
evidence is persisted before the caller-visible artifact name is projected.
Evidence-backed restart can therefore finish or verify publication without
re-executing a nondeterministic build or treating an artifact pathname as
historical truth. Construction/check
restart validates those retained sessions under the exact retained
transaction/progress/node authority; it does not reconsult current
construction/check configuration, the fresh session locator, or retained-installed-package lookup. A started operation likewise retains the complete admitted
operation session before an effect journal may name it, together with the exact
incoming-authority-to-artifact-path binding needed by application restart. Operation
restart decodes that historical session and archive binding without replaying live
operation specification, target observation, predecessor construction, or collection
authority. Interrupted applications are reopened through the direct
`libpkgapply-posix` request-to-journal index. Resume never reacquires
collections, asks the operator to restate transaction semantics, substitutes
current state for historical state, scans for journals, or silently replans. Private command evidence has one current admitted format. Bytes that do not
belong to that format are rejected rather than interpreted through a compatibility
path. Command evidence uses libpkgexec-owned backend-profile encodings for the
admitted construction/check/lifecycle profiles. Historical construction/check result
evidence retains the exact owner-encoded profile body with each durable attempt.
Historical lifecycle results likewise carry the exact libpkgexec-owned profile bytes
inside the libpkgapply-exec owner encoding. None of those completed result paths consults
a current backend or injects a command-level profile as historical decode authority. Current interpreter observation, backend capability, and supervisor-credential
preflight are execute-now authority required only for construction, check, or lifecycle work that can still
execute; an operation-only, completed, or already externally blocked run does not
reacquire unused execute-now process authority merely to explain durable history. The
retained interpreter identity is historical evidence authority, not an observation that the old pathname still exists.
Target observations are live only for the current operation dispatch.

Construction/check and lifecycle execution keep separate existing root views
and explicit numeric credential sets. Supplying the same roots or credentials
is an explicit caller policy, not an authority collapse inside the controller.
The current Linux backend admits only the invoking supervisor's credentials;
`pkgctl` refuses incompatible explicit credentials before run admission rather
than pretending to provide fakeroot or ownership virtualization.

All authority remains explicit: existing runtime and construction/check roots;
operation-capable transactions additionally require existing lifecycle and target
roots and their target-operation stores. One exact interpreter authority is
required when current execution is possible, alongside explicit numeric
credentials, a start-only complete build policy, retained installed-package trees,
canonical state binding, and a caller-issued run nonce. The configurable policy
surface is parallelism plus source-date epoch; pkgctl fixes umask 0022,
package-root layout, C.UTF-8/UTC, denied network, and isolated HOME. Resume
recovers that admitted policy instead of asking the caller to restate it. The command creates no namespace, starts no daemon, waits on no timer,
loops beyond the bound, retries implicitly, rolls back, repairs, cleans up, or
collects history. At 0.35.0 this closed the functional package-management
chain; later releases reduce construction-only authority and expose that path
through the constrained build frontend.

The package-construction frontend is backed by the same controller core already
qualified in-process against disposable roots. That campaign drives
real acquisition, resolution, dependency construction, checking, target
observation, installation, protected upgrade, rejected-object evidence, state
publication, exact-convergence removal, and reconciliation-store persistence.
It also crosses durable application/publication restart boundaries and separate
pre-operation and operation failure matrices: a completed POSIX application whose
exact receipt body was durably retained is adopted into the controller journal
rather than applied or resumed again, already-selected canonical state is
reconciled without duplicate publication, and definitive construction/check
failures block dependent work before target mutation. The campaign also launches
successful and failed sealed transactions through
`native_posix_transaction_run_runtime`. It destroys and reopens the successful
runtime to prove completed retained operation sessions and effect bodies replay without
a second live operation specification, target observation, archive-path discovery, or mutation; reopens failed build/check journals
without rerunning the actuator or acquiring operation/archive authority; and
reopens terminal application, pre/post-lifecycle, and publication failures without
retry, rollback, or republication. The latter cases preserve the exact physical
truth: application/pre-lifecycle failure leaves the target unchanged, while
post-lifecycle/publication failure may leave the application present with
canonical state still at its prior generation. A separate uncertainty matrix
distinguishes durable publication intent from a terminal indeterminate publication
receipt. Publication intent reconciles without
republishing when authoritative state already exposes the exact result, and may
retry the exact retained request while authoritative state still exposes the
exact prior generation. A terminal indeterminate receipt with that prior state
remains externally blocked instead of being discarded and retried. An
interrupted lifecycle intent is likewise externally blocked across reopen with
zero durable advancement or physical replay. A separate outer-lease matrix
revokes the runtime's real POSIX mutation lease by unlinking its anchored lock
file after a successful post-install lifecycle action and during a successful
state publication. Lease loss is retained once as a non-retiring dispatch
observation. The bounded drive that retains that observation reports external
resolution immediately, and every later drive/reopen reports the same block
instead of resubmitting the observation, reacquiring archive authority, or
promoting a publication completed after ownership was lost. A complementary
lease-contention matrix holds the same real POSIX exclusion domain from another
controller. `lock_busy` is translated into the stable
`mutation-authority-unavailable` control disposition rather than package/effect
failure. Fresh contention releases the unstarted reservation and admits no
effect attempt; recovery contention preserves the already-started run/effect
heads with zero durable advancement. Neither call waits or retries. A later
explicit drive may proceed after the competing holder releases. The privileged
CLI vertical independently holds the exact command-derived POSIX exclusion
domain before `pkgctl run --start`: the admitted run must report
`mutation-authority-unavailable`, retain one released-unstarted operation
reservation with no effect attempt, refuse a duplicate `--start`, and complete
only after an explicit `--resume` once the holder releases. That resume is also
required to succeed after the live collection is removed, proving the block did
not create hidden rediscovery authority. A sibling privileged recovery vertical
interrupts after durable application intent, then holds that same command-derived
POSIX exclusion domain across `--resume`. The blocked resume must report
`mutation-authority-unavailable` with zero durable advancement while preserving
the exact started operation dispatch and effect head. After explicit holder
release, a later `--resume` must complete that same dispatch/effect rather than
reserving a replacement, even after live collection bytes are removed. A third
privileged vertical revokes the command's real anchored POSIX lease while a
post-install lifecycle action is still active. The admitted `--start` must retain
one non-retiring `outer-lease-lost` observation and report
`external-resolution-required`: application and post-install effects remain on
the target, canonical state remains at the prior generation, and publication
never starts. Repeated `--resume` after live collection removal must preserve the
same run/effect heads, perform zero durable advancement, and must not recreate a
target-lock file. Only the external process actuator and the explicitly faulted
owner protocol are replaced in the in-process campaign.
This test composition does not make reconciliation a production `pkgctl`
dependency.

Release 0.33.0 supplies native fresh-operation and restart authority without
actuating the target. One replayable per-dispatch specification source supplies
exact target and planning authority plus explicit lifecycle execution order for
the current operation, while fixed transaction/lifecycle configuration supplies
coordinates and credentials. The existing effect boundary validates the order
against the exact lifecycle-node set attached by transaction phase edges; graph
storage order is not treated as execution precedence. The native source prepares
through the existing owner boundaries and admits each deterministic lifecycle
scratch session as a direct child of the caller-provisioned session root. The
child identity is keyed by the stable journal, dispatch, lifecycle phase, and
phase index, so the operation-authority boundary performs no filesystem
materialization while `libpkgapply-exec` still receives the pre-existing parent
it requires. The source then issues a mechanical attempt nonce for one exact
reserved head.

Restart reconstructs the same session, selects the exact run-retained latest
effect record, and asks a caller-owned body source for semantic values the
effect journal deliberately records only by identity. The existing restart
checkpoint validates those bodies. Incoming archives are resolved only through
an explicit incoming-authority-to-path map and opened with the exact retained
archive digest through a caller-selected image backend.

The implementation performs no target observation, execution, mutation,
journal append, state read, discovery, retry, scheduling, or command actuation.
At that boundary the executable remained read-only and native runtime
composition was still external; Release 0.34.0 closes that composition. One
narrow mutating command remains.

Release 0.32.0 supplies exact semantic transaction-progress rehydration.
One store-backed source begins from the sealed transaction, selects only
completed dispatch history, and replays each terminal fact when its graph unit
becomes ready. Construction and check evidence is selected by exact journal,
dispatch, and attempt identities and decoded through the existing owner codecs
under caller-owned complete bodies. Completed operations reopen only from exact
terminal effect evidence.

Successful publication advances the immutable state epoch through
`libpkgstate::project_publication_request()`. The controller neither copies
state-delta rules nor reads canonical storage to reconstruct a historical
epoch. Reserved, started, released, and indeterminate dispatches remain control
ownership rather than semantic facts. The reconstructed progress identity,
current state, completion, and failure state must reproduce the durable run
record exactly.

The implementation performs no execution, continuation, append, publication,
retry, repair, target observation, or filesystem mutation. Releases 0.33.0 and
0.34.0 subsequently close operation/archive authority and native runtime
composition. One narrow mutating command remains.

Release 0.31.0 supplies the first native implementation of the shared construction/check session authority.
It derives exact admitted sessions from retained transaction, progress, journal,
dispatch, explicit roots and policy, and retained predecessor or installed
package resources without filesystem observation or mutation.

Release 0.29.0 closes the semantic half of construction/check restart
recovery without moving authority into the durable store. The runtime selects
one exact evidence record by run journal, dispatch, and attempt session, asks a
caller-owned context source for the original admitted session, execution
request, backend profile, and source materialization where required, validates
those bodies against every retained identity, and invokes the existing
canonical build/check decoder. The controller result is accepted only when its
canonical identity is reproduced exactly.

Missing evidence remains unresolved started work. Foreign context and
contradictory decoded evidence fail closed; no identity is promoted into a
semantic body. Operation recovery remains owned by the effect-journal path.
The caller still owns discovery and realization of the context bodies, so this
release adds no path scan, resource materializer, process adoption, scheduler,
retry loop, or mutating command. The command surface remains read-only.

Release 0.28.0 added the preceding durable evidence barrier. Construction and
check execution publish the exact canonical subordinate result encoding as an
immutable content object and then publish one typed journal/dispatch/attempt
index before terminal run retirement. The caller-configured POSIX runtime
retains four directory authorities: run journal, construction/check evidence,
effect journal, and target lock.

Release 0.27.0 makes construction inputs resolver-issued authority rather
than caller-written digest bundles. Concrete package resources are admitted only
for one exact build or check execution, operation preparation consumes the
retained build/image admission, and the standalone build-plan projection owns
planner translation. The effect journal now has one first-generation encoding
with a mandatory durable head; no undeployed compatibility lineage remains.

Release 0.26.0 separates caller run intent from mechanical dispatch nonce
issuance. `posix_transaction_run_runtime::launch()` now receives one explicit
`transaction_run_nonce`; supplying the same nonce retries the same durable
history, while another nonce intentionally selects another history.

Fresh dispatch nonces no longer require a caller service. The stateless
`canonical_transaction_dispatch_nonce_source` validates the exact committed
record/run pair and derives one domain-separated nonce from that head. Exact
retries derive the same nonce, while every committed successor establishes a
new issuance domain. The derivation is not randomness authority and stores no
hidden state. Semantic rehydration, execution/recovery materialization, archive
lookup, backends, and canonical state remain caller-owned.

Release 0.25.0 assembles the existing durable run controller into one
caller-configured POSIX transaction runtime.
`posix_transaction_run_runtime` retains three caller-opened directory
authorities for the transaction-run journal, effect-attempt journal, and target
mutation locks. It owns the two POSIX journal stores, native construction and
check drivers, and the concrete per-dispatch effect source.

At its 0.25.0 boundary the runtime still borrowed replay-safe run and dispatch
nonce sources together with semantic progression and execution/recovery
sources, archive lookup, physical execution backends, and the canonical state
store. Release 0.26.0 removes those nonce-source dependencies without moving
caller run intent into the runtime.
`launch()` admits or resumes one exact caller-supplied progression and drives it
under one positive bound. `drive()` advances only one exact caller-supplied
journal identity under one positive bound. Neither method discovers journals,
initializes stores, loops beyond the bound, schedules workers, retries failures,
selects backends, or exposes a mutating command.

Release 0.24.0 provides the first concrete native per-dispatch effect source.
`posix_transaction_effect_driver_source` is deliberately caller-configured: the
caller chooses the application and lifecycle backends, canonical state store,
replayable-archive source, and target lock directory. The source duplicates the
directory descriptor, acquires one fresh nonblocking POSIX outer lease for the
exact handoff, and derives the application projection from canonical state
through `libpkgstate-apply` while that lease is live.

Incoming archives are opened through caller authority and admitted only when
both package-image and inspection-receipt identities match the sealed incoming
package authority. Fresh continuation and resulting-state observation share one
lease lifetime; terminal state observation and publication reconciliation
receive only their classifier-selected target-scoped authority. The library
still discovers no paths, credentials, archives, backends, retries, or policy,
and the executable exposes no mutating command.

Release 0.23.0 separates per-dispatch effect continuation from canonical-state
observation and publication reconciliation. Fresh operation execution now
requires a call-scoped continuation driver and a distinct resulting-state
observer bound to the same live target lease. Recovery requests exactly the
physical authority implied by the durable effect checkpoint: continuation,
state observation, publication reconciliation, or none.

The split prevents post-publication recovery from fabricating the old
lease-bound application projection merely to read the current canonical state.
Publication reconciliation uses only a target-scoped lease, the exact retained
publication request, and the canonical store. Successful terminal recovery can
read resulting state without receiving lifecycle, application, or publication
authority. No concrete native source, policy discovery, scheduler, or mutating
command is added.

Release 0.22.0 establishes per-dispatch effect-driver authority for bounded
transaction-run advancement. A caller-owned `transaction_effect_driver_source`
receives the exact validated execution or recovery handoff and returns one
call-scoped driver for that operation. Fresh acquisition occurs after the
reservation is durable and before effect admission; recovery acquires physical
authority only when continuation can touch the target or a successful result
requires a resulting-state read.

The controller validates the returned live mutation lease and exact state
projection before subordinate effect admission. Source refusal or invalid
authority leaves the dispatch reserved and creates no effect journal. Drivers
are not retained, serialized, reused across operations, or discovered by the
frontend. No native backend assembly or mutating command is added.

Release 0.21.0 exposes exact durable effect-attempt inspection on the read-only
command surface:

```sh
pkgctl inspect-effect --effect-store /path/to/effect-store --attempt SHA256
```

The command opens only the explicitly named existing POSIX store and loads only
the explicitly named attempt. It delegates classification to
`inspect_effect_attempt()` and output to `render_report()`; it does not scan for
attempts, traverse run journals, rehydrate subordinate semantics, construct
restart authority, invoke a driver, append, reconcile, repair, or mutate
anything. Effect-journal failures retain their typed diagnostics, and read-only
store access does not create or open the writer lock for writing.

Release 0.20.0 established the underlying durable sensor. It pairs one
storage-derived `effect_attempt_record` with the existing pure
`effect_restart_assessment` and reports controller-owned record, predecessor,
lifecycle, application, transaction, publication, terminal, and reconciled
state identities without promoting any identity into semantic evidence.

Release 0.19.0 exposes exact durable transaction-run inspection on the
read-only command surface:

```sh
pkgctl inspect-run --run-store /var/lib/pkgctl/runs --journal SHA256
```

The command opens only the explicitly named existing POSIX store and loads only
the explicitly named journal. It delegates classification to
`inspect_transaction_run()` and output to `render_report()`; it does not scan for
runs, infer semantic progression, inspect effect journals, append, reserve,
execute, repair, or mutate anything. Read-only store access does not create or
open the writer lock for writing.

Release 0.18.0 established the underlying durable sensor. It classifies one
storage-derived head as completed, stopped after failure, active, or quiescent
incomplete and retains exact restart dispositions without promoting identities
into semantic evidence.

Release 0.17.0 establishes restart-safe transaction launch:

```text
exact initial progress + policy
              |
              v
   replay-safe run nonce
              |
              v
 derive exact journal identity
              |
      +-------+--------+
      |                |
 no committed head   existing exact head
      |                |
 append sequence zero  resume it
      +-------+--------+
              |
              v
      bounded serial drive
```

`launch_transaction_run()` composes admission and bounded driving without
replaying admission over an advanced journal. It derives the exact journal from
the immutable initial run and caller-owned run nonce, loads that journal head,
and appends sequence zero only when no committed head exists. An existing head
must retain the same transaction, run nonce, and dispatch policy before it may
be resumed.

A failure before the admission append performs no drive action. A failure after
admission leaves sequence zero or a later ownership record durable for an exact
retry. Retrying after partial or completed work loads the current head and does
not republish sequence zero, consume fresh dispatch authority for terminal work,
or invoke a completed driver again. The launch remains explicitly bounded by
`transaction_run_drive_policy`.

Release 0.17.0 adds no journal discovery, unbounded execution, worker,
concurrency, adaptive scheduling, retry timing, backoff, resource or evidence
discovery, process adoption, rollback, cleanup, compaction, garbage collection,
or mutating CLI command.

Release 0.16.0 establishes the durable origin of one transaction-run history:

```text
exact initial progress + dispatch policy
                |
                v
       immutable initial run
                |
                v
 replay-safe caller nonce authority
                |
                v
      sequence-zero record
                |
                v
       durable store append
                |
                v
 storage-derived reopened run
```

`admit_transaction_run()` constructs one exact initial run, obtains a run nonce
from an injected `transaction_run_nonce_source`, commits sequence zero, validates
the authority returned by storage, and reopens the run from that committed
record. Exact retries for the same initial run must receive the same nonce and
converge on the same record. A nonce-source refusal performs no store write; a
store failure grants no controller authority.

Admission reserves no unit, acquires no execution resources, invokes no driver,
and performs no effect-journal operation. It adds no scheduler, drive loop,
retry timing, discovery, rollback, cleanup, compaction, garbage collection, or
mutating CLI command.

Release 0.15.0 adds an explicitly bounded serial transaction drive without
turning the control plane into a scheduler:

```text
committed run head
        |
        v
reconcile retained ownership, if any
        |
        `-- otherwise issue nonce for this exact head
                         |
                         v
                 reserve and execute one
                         |
                         v
                 classify durable outcome
                         |
            +------------+-------------+
            |                          |
        stop condition             bound remains
            |                          |
          return                 reload head
```

`drive_transaction_run()` accepts a positive maximum step count and repeatedly
invokes the one-step authority. Every iteration reloads the committed head.
Completion, containment failure, external-resolution authority, incomplete
quiescence, or exhaustion of the explicit bound stops the call.

Fresh dispatch nonces come from an injected
`transaction_dispatch_nonce_source` keyed to the storage-derived record and
rehydrated run. The source is not called while retained ownership is being
reconciled or when the run is stopped, complete, or otherwise has no ready work.
Exact retries against an unchanged head must return the same nonce; a committed
successor head may yield a different nonce. This prevents speculative nonce
preallocation from consuming attempt identity outside durable ownership.

The returned drive result retains the ordered one-step outcomes and validates
that continued steps remain in one journal, follow strictly increasing durable
heads, and never continue after a stopping result. Release 0.15.0 adds no
worker, concurrency, adaptive priority, unbounded loop, retry timing, backoff,
resource or evidence discovery, rollback, cleanup policy, compaction, garbage
collection, or mutating CLI command.

Release 0.14.0 composes the durable run, exact authority, execution, and
reconciliation boundaries into one bounded transaction advancement:

```text
committed run head
        |
        v
exact progression rehydration
        |
        +-- retained ownership --> reconcile one dispatch --> return
        |
        `-- quiescent -----------> reserve one dispatch
                                      |
                                      v
                              commit reservation
                                      |
                                      v
                              acquire exact authority
                                      |
                                      v
                              execute and commit --> return
```

`advance_transaction_run_once()` loads the committed head selected by an exact
journal identity. It rehydrates semantic progression through the caller-owned
source and gives the first active dispatch in durable ledger order absolute
precedence. Reserved ownership is released; started construction or check work
accepts exact recovered evidence; started operation work inspects the retained
effect attempt. External-resolution effect state returns without a driver call
or journal append. No fresh work is reserved in the same call.

Only a quiescent reopened run may reserve the first canonical ready dispatch.
The selected driver and effect-store requirements are validated before the
reservation append. The reservation is then committed before fresh execution
authority is requested, and the existing write-ahead execution functions retain
start and terminal crash barriers. An authority-source or driver failure therefore
leaves exact reserved or started ownership rather than an invisible partial step.

The returned result is reconstructed from storage-derived run authority and
validates its disposition, retained dispatch, and semantic evidence. Release
0.14.0 adds no loop, scheduler, worker, concurrency, retry or backoff policy,
resource or evidence discovery, process adoption, rollback, cleanup policy,
compaction, garbage collection, or effectful CLI command.

Release 0.13.0 closes the authority handoff required before a deterministic
transaction runner can exist:

```text
durable run identity
        |
        v
caller-owned progression/resource/recovery source
        |
        v
exact pkgctl validation
        |
        v
sealed execution or recovery handoff
```

`rehydrate_transaction_run()` obtains one complete semantic progression from an
injected source and reopens only the exact run named by the durable record.
`acquire_transaction_dispatch_execution_authority()` obtains fresh concrete
construction, check, or operation resources for one exact reserved dispatch and
validates them through the existing pure start transition. It stores nothing and
invokes no driver.

`acquire_transaction_dispatch_recovery_authority()` obtains exact subordinate
evidence for one restart-classified active dispatch. Started construction and
check evidence must retain the exact admitted session already named by the run.
Operation recovery must retain both the exact effect session and effect-attempt
identity. A never-started reservation requires no evidence source call.

Fresh resources may vary where their owning admission contract declares paths or
other coordinates non-semantic. Restart evidence may not vary: it must identify
the exact durable attempt. Release 0.13.0 adds no journal access, reservation,
execution, reconciliation, evidence discovery, scheduler, loop, retry policy,
rollback, compaction, garbage collection, or effectful CLI command.

Release 0.12.0 closes the durable restart actuator for one exact dispatch:

```text
committed run restart checkpoint
        |
        v
validate caller-rehydrated recovery authority
        |
        v
release reserved work | accept build/check evidence | continue effect journal
        |
        v
commit one exact run successor or stop for external resolution
```

`reconcile_reserved_dispatch_durable()` durably releases only a dispatch that
restart classified as reserved and never started.
`reconcile_construction_dispatch_durable()` and
`reconcile_check_dispatch_durable()` accept caller-rehydrated terminal evidence
only when its admitted session is the exact session retained by started
ownership. They submit through the existing completion functions and commit one
validated run successor. A lost final append is exactly retryable.

`reconcile_operation_dispatch_durable()` verifies the run-retained effect
attempt against the latest exact effect-journal record. Automatically
continuable stages resume through the existing effect restart authority. A
terminal effect journal can repair a lost run completion without invoking the
mutation driver again. Stages requiring external resolution advance neither
journal nor run and invoke no driver. Successful continuation rereads canonical
installed state before progression accepts the result.

The journal still does not reconstruct semantic evidence. Construction and check
results, effect restart checkpoints, drivers, leases, resources, and stores are
caller-supplied exact authorities. Release 0.12.0 adds no work selection,
scheduler, execution loop, evidence discovery, retry timing, process adoption,
rollback, compaction, garbage collection, or effectful CLI command.

Release 0.11.0 closes one explicitly selected dispatch behind the durable
transaction-run barrier:

```text
reserved run snapshot
        |
        v
commit started ownership
        |
        v
invoke one injected driver
        |
        v
commit terminal evidence or retained uncertainty
```

`execute_construction_dispatch_durable()` and
`execute_check_dispatch_durable()` commit the exact started-run successor before
their driver is invoked. They then submit the existing terminal controller
evidence through the ordinary dispatch completion functions and commit that one
successor. A failed start append invokes no driver. Driver escape or failed
terminal commitment fabricates no completion: the committed started dispatch
remains the restart authority.

`execute_operation_dispatch_durable()` preserves the stricter cross-journal
order. It commits effect-attempt admission, commits the started run retaining
that attempt, executes through the existing durable effect journal, and only
then commits the resulting run successor. Successful publication is paired with
a fresh authoritative state read. Lost-lease and indeterminate-publication
results remain ordered observations on an active dispatch. If the effect journal
reaches terminal state but the final run append is lost, restart still names the
exact effect journal to inspect.

Release 0.11.0 does not select or reserve work. It creates no loop, thread,
backend, resource-discovery policy, retry policy, scheduler, transaction-wide
cancellation, rollback, or effectful CLI command. The caller supplies one exact
reservation, admitted session, driver, and both stores.

Release 0.10.0 established durable transaction-run ownership and conservative
restart classification:

```text
exact transaction_progress + immutable dispatch ledger
        |
        v
immutable, single-transition run snapshots
        |
        v
checksummed committed head
        |
        v
exact progression rehydration
        |
        v
release reserved work | recover build/check | inspect effect journal
```

A durable run history begins before the first reservation, so sequence zero
contains no dispatch ownership and every positive sequence retains at least
one reservation. Its sequence must equal the transition count derivable from
retained dispatch states and uncertainty observations. Every successor snapshot
contains exactly one legal ledger transition: one reservation, one start, one
unstarted release, one uncertainty observation, or one terminal completion. A
reservation successor must bind the predecessor's exact progression identity
and state epoch. Records bind the transaction, dispatch policy, caller-issued
run nonce, sequence, predecessor, current state epoch, progression identity, and
complete immutable dispatch history. The POSIX store publishes immutable record
files and then atomically advances a checksummed read-only head. Recovery opens
only the exact self-contained snapshot named by that committed head; exact crash
retries are idempotent.

The journal does not serialize package truth. After restart, the caller must
rehydrate the exact `transaction_progress` from authoritative construction,
check, effect, and state evidence. `reopen()` verifies that authority and then
restores dispatch ownership. A digest in a journal record is never promoted into
a build result, check result, effect result, or installed-state snapshot.

Restart classification is conservative. A durably `reserved` dispatch may be
released because no execution session was admitted. A durably `started`
construction or check requires external recovery evidence. A started operation
retains the exact effect-attempt identity whose journal must be inspected.
`commit_operation_dispatch_start()` commits that effect admission first and the
started run snapshot second. Only after both commits may an effect driver run.
An interrupted exact call can be retried; an orphan admission does not promote a
still-reserved dispatch into started work.

Release 0.9.1 established the committed-head protocol for the effect-attempt
store. A head-selected snapshot must carry the exact sequence derivable from its
retained effect evidence and cannot represent lease loss over an unresolved
publication intent. Release 0.10.0 reuses that lower commit point rather than
defining a second interpretation of effect durability.

Release 0.9.0 established immutable transaction dispatch and in-flight
ownership. A caller-issued 32-byte nonce distinguishes each physical dispatch
attempt. `reserve_next()` chooses the first canonical ready unit allowed by
explicit construction/check capacities, one hard operation lane,
graph-member exclusion, and stop-after-terminal-failure containment. The
returned `transaction_run` retains every reservation for the lifetime of the
run, so a nonce or graph unit cannot be silently dispatched twice.

Reservation is not execution. A unit becomes `started` only after an exact
construction, check, or operation session is admitted against the retained
transaction and predecessor evidence. Unstarted work can be released. Started
work cannot be called unstarted again. Construction and check completion retire
only the exact started session. Definitive operation evidence retires the
operation; lost-lease and indeterminate-publication evidence remains an active
observation until authoritative resolution arrives.

The ledger binds exact successful predecessor evidence. Check-scoped package
inputs are ordered before construction by `libpkgtransaction >= 4.0.0`, then
verified against the retained build result and artifact before a construction
session starts. Retained installed inputs must still be the same exact package
in the current state. Failure containment prevents both new reservations and
starting merely reserved work after terminal failure, while already-started
independent work may still submit exact terminal evidence.

Release 0.8.0 closed exact check request, concrete session, execution, and
terminal progression evidence. Dispatch composes those existing boundaries; it
does not invoke them automatically.

Release 0.7.1 established evidence-driven transaction progression and
current-state epochs. A target action and its exact pre- and post-lifecycle
phase nodes form one operation unit, while build and check nodes remain exact
individual units. Independent ready units remain visible together; `pkgctl`
does not choose among them.

Release 0.6.0 established one-operation preparation between completed
construction and the target-effect kernel. Release 0.5.0 established one exact
package-construction session, and 0.4.0 closed the restart loop for one target
mutation sequence.

The executable exposes this command surface:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
pkgctl run
pkgctl build
pkgctl inspect-run
pkgctl inspect-effect
```

Every collection root, target-state binding identity, architecture, goal scope,
selected durable journal, and destructive convergence choice is explicit.
`transaction` defaults to `preserve-unselected`; exact convergence requires
`--converge-exact`. Catalog, resolution, transaction composition, and both
inspection commands are read-only. `run` enters the general native transaction
runtime; `build` enters the construction/check-only shape with its explicit
public artifact root.

`pkgctl run` is the sole effect-implying command in 0.35.0. It composes one
sealed transaction and advances one exact durable run under the caller's
positive bound. Native execution capability absence is rejected as control-plane
unavailability before a fresh transaction is retained or admitted; it is not
recorded as a package build/check failure. `--start` and `--resume` are
deliberately distinct; neither can silently become the other. The command exposes
no unbounded executor, daemon,
implicit retry, adaptive scheduler, transaction-wide rollback, journal scan,
store initialization, repair, compaction, or garbage collection.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, deterministic dispatch reservation, in-flight ownership,
evidence-driven transaction progression, current-state epoch binding,
durable controller-attempt snapshots, one-dispatch write-ahead execution,
exact one-dispatch restart reconciliation, exact run-authority rehydration,
conservative restart classification,
outer-lease observation, transaction-evidence composition, deterministic
diagnostics, and presentation.

The following meanings remain external:

- recipe and profile syntax: `libpkgsource-yaml`;
  the executable reports its typed parser diagnostics directly, while the
  controller core does not link the syntax adapter;
- available package universe: `libpkgcatalog` and
  `libpkgcatalog-acquire`;
- installed truth, planner projection, and publication: `libpkgstate`,
  `libpkgstate-plan`, and `libpkgstate-apply`;
- exact dependency selection: `libpkgresolve`;
- cross-package operation composition: `libpkgtransaction`;
- pure check requests and terminal evidence: `libpkgcheck`;
- concrete check execution admission and realization: `libpkgcheck-exec`;
- candidate and artifact planner projection: `libpkgsource-plan` and
  `libpkgbuild-plan`;
- package-local filesystem intent and mutation: `libpkgplan` and
  `libpkgapply`;
- lifecycle-node derivation and execution: `libpkgapply-exec` and
  `libpkgexec`;
- source acquisition and verification: `libpkgfetch`;
- build request/result and execution realization: `libpkgbuild` and
  `libpkgbuild-exec`;
- package-image authority: `libpkgimage`.

`transaction_progress` does not infer transaction order beyond the exact
transaction graph and exposes every ready unit. The dispatch ledger may reserve
the first canonical ready unit allowed by explicit capacity and failure policy;
it does not invent graph edges or execute the reservation. For lifecycle nodes
not ordered relative to each other by that graph, the caller supplies an exact
order and the controller binds it into the effect request identity.

## Building

`pkgctl` requires Meson 1.6.0 or newer.

```sh
meson setup build \
  -Dlink_mode=shared \
  -Dman_pages=enabled
meson compile -C build
meson test -C build --print-errorlogs
```

Use `-Dlink_mode=static` for a static authority closure where all external
static dependencies are available.

See `DESIGN.md` for the normative controller boundary and `TESTING.md` for the
qualification contract.
