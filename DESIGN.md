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

## Current construction/check process-death boundary

Construction and check attempts use write-ahead admission. Before a started run
record may become durable, the controller publishes an immutable attempt record
containing the exact admitted session encoding and its transaction, node,
dispatch, and request bindings. Terminal result evidence remains distinct.

Restart therefore has two closed authorities for a durably started dispatch:

```text
terminal result evidence present  -> decode exact retained result
terminal result evidence absent   -> decode exact retained attempt and replay
```

Missing both records fails closed. Replay does not consult the live session
locator, collection, current configuration, or retained-installed-package
lookup. An attempt record published before a failed started-run commit is merely
unreferenced private evidence; reserved ownership is still released by the run
journal rules.

This closes process death before controller result evidence. Successful native
construction also separates private sealing from public artifact projection:
the exact verified archive remains beneath the admitted construction session,
terminal construction evidence becomes durable, and only then may the public
artifact name be projected. Recovery with terminal evidence publishes or
verifies that exact retained artifact without rerunning construction. A file
discovered in an artifact directory is therefore never allowed to become
historical success without retained controller result authority.

## Empty-target feedback qualification

The synthetic rootfs campaign is not a new controller verb. It treats an empty
managed target as an ordinary desired-state convergence problem, then hands the
terminal canonical state and target root to `libpkgaudit` as an independent
test-only observer. The controller does not call the auditor in production and
does not reinterpret audit findings as transaction evidence. A clean audit
closes the qualification loop; deliberate post-terminal deletion of one owned
object must be observed as drift without changing the retained transaction or
canonical state.

This boundary keeps deployment composition outside `pkgctl`: a future rootfs
client may select policy and invoke convergence plus audit, but package source,
resolution, transaction, state, and observation authority remain with their
owners.

## Release 0.37.0 build frontend authority

`pkgctl build PACKAGE` is not a second construction orchestrator. It constructs
one ordinary transaction request and enters the same durable transaction-run
kernel used by `pkgctl run`. The frontend owns only policy that belongs to its
public verb: the exact package is a build goal, `--check` adds the corresponding
check goal, and catalog authority is preferred. The composed transaction is then
validated as frontend authority: it must retain exactly that package build goal,
optional same-package check goal, preserve-unselected convergence, and a
catalog-backed build node for the direct subject (plus its check node when
requested). Installed state therefore cannot silently satisfy the public build
verb without construction. Caller-supplied goals, convergence policy, and
target-operation authority are invalid. A direct package goal is resolver-
target-qualified even when its scope is `build`; only dependencies admitted by
that scope are build-environment selections. Frontend validation therefore binds
the direct build/check nodes to the exact resolved goal-member selection identity
and catalog candidate, rather than treating the resolver environment label as the
construction authority. Target qualification of that selection does not grant a
target-operation node or target mutation authority.

The build frontend has an explicit public artifact authority distinct from the
private runtime hierarchy. `--artifact-root` names an existing absolute normalized root
used by the ordinary native construction-session locator. The session root set
must remain pairwise disjoint, and build additionally refuses lexical overlap
between the public artifact root and the private runtime root. The frontend kind
and exact artifact-root coordinate are retained in immutable command evidence
with the transaction universe and execution authority. Resume must present the
same frontend and artifact coordinate before the transaction can advance; it
cannot redirect future publication or reinterpret retained `run` evidence as a
`build` request.

A build command is therefore structurally incapable of target mutation. Its
command object has no lifecycle root, target root, or lifecycle credentials, and
a recomposed build transaction containing target-operation nodes is refused. The
canonical state store remains resolution input only. Successful construction
results are projected from durable transaction progress into a deterministic
artifact report; no new artifact database, scan, or truth-reconstruction layer is
introduced. The reported path and identities describe admitted historical
artifact authority, not a fresh observation that those bytes still exist.
Terminal replay derives the same report from retained construction evidence even
when the live collection has disappeared and does not reopen completed archives
to manufacture present truth.

## Release 0.36.0 construction-only runtime authority

A sealed transaction that contains only construction, check, and retain nodes
does not acquire target-operation capability merely because the generic durable
run kernel can also execute operations. Its native composition carries the run,
construction/check evidence, and effect namespaces; the native construction/check
session source; retained installed-package tree source; construction/check
execution mechanisms; and package archive backend. The target-lock namespace,
live operation specification source, operation restart bodies, retained operation
sessions, application and lifecycle mechanisms, canonical-state publication
backend, and operation effect-body sink are absent.

The reduced shape is an invariant, not a convenience path. The two-argument
native runtime configuration refuses a transaction containing install, upgrade,
remove, or lifecycle authority. The operation-capable configuration refuses a
construction/check-only transaction. Native runtime assembly then refuses either
surplus target-operation authority on the reduced shape or an incomplete
operation-capable shape. Generic execution and recovery composition represents
the operation source with an optional pointer and fails closed only if an
impossible operation dispatch reaches a construction-only composition; no dummy
operation authority is fabricated.

The bounded command selects the shape only after the exact transaction is
composed or recomposed. For a construction/check-only graph it does not open the
lifecycle root, target root, target-lock namespace, application journal/checkpoint
or payload/capture stores, rejected/completed stores, effect-body store, or live
canonical state publication backend. The canonical store remains semantic input
to transaction resolution/recomposition; it is not thereby granted publication
authority. This keeps `pkgctl run --goal build=...` on the same durable run kernel
while removing the target mutation capability that the selected graph cannot use.

## Pre-frontend live-target qualification boundary

The in-process package campaign shares live target-observation semantics with the
native runtime. `pkgctl-core` owns one translation from descriptor-anchored
`libpkgapply-posix` observations into complete `libpkgplan` target facts and
seals them in `pkgctl/native-target-observations/1`. The CLI retains and replays
those values, but no longer owns the object-kind, metadata, content, or device
conversion table.

This lets package qualification exercise install, protected upgrade, rejected
object publication, canonical state replacement, exact-convergence removal, and
restart-safe reconciliation persistence without first adding another frontend
command. Reconciliation remains an adjacent library seam in this campaign: the
exact completed application evidence is projected and persisted directly by the
`libpkgreconcile*` owners. `pkgctl` does not acquire a production reconciliation
dependency until it owns an actual coordination decision beyond forwarding that
evidence.

The campaign also qualifies failure and restart as authority composition, not as
frontend behavior. Build/check failures enter through the execution backend and
ordinary terminal dispatch completion, so graph progression owns which dependent
work becomes blocked. Application/publication interruption is injected only at an
effect-journal append boundary after the corresponding subordinate owner may have
already committed its side effect. Reopen then uses the production run/effect
restart checkpoints and `reconcile_operation_dispatch_durable()`: completed POSIX
application journals whose exact receipt body is already durable are adopted into
the controller journal without a fresh apply or application resume. Their
completed evidence remains bound to the exact historical lease projection body
retained by the application journal. A newly held lease guards one current
canonical-state observation, but that present-tense snapshot is never used to
reconstruct historical projection truth. Genuinely resumable subordinate journals
still use `libpkgapply` continuation and bind new terminal evidence to the current
projection. An
already-selected canonical publication is observed without publishing the same
state again.

The package campaign also crosses the production native composition root itself.
A bounded `native_posix_transaction_run_runtime` launch executes the real package
transaction through construction, check, application, and publication. The
caller retains the exact operation observation set before target mutation and
serves it again when a newly constructed runtime rehydrates the completed
journal; subordinate terminal bodies are likewise supplied by the caller-owned
restart-body source. Reopening therefore reproduces completed progress and stops
quiescent without acquiring the incoming archive or mutating target/state a
second time. The same composition root also owns the build/check failure
containment campaign: a definitive failure commits ordinary subordinate evidence,
stops dependent progression, and a newly constructed runtime rehydrates that
stopped journal without rerunning construction/check or crossing into operation,
archive, effect-body, target, or publication authority. Definitive operation
failures cross the same root through their owning protocols. A failed physical
application or pre-install lifecycle action stops before target mutation; a
failed post-install lifecycle action or failed canonical publication can stop
after application, so the target may contain the selected files while canonical
state still names the prior generation. Those facts are terminal evidence, not
implicit rollback instructions. Reopening the failed runtime must reproduce the
same effect identity and progress from retained subordinate bodies without
rerunning lifecycle/application work, reacquiring the archive, rolling back the
target, or publishing state. Non-terminal uncertainty crosses the same root but
never masquerades as terminal failure. A durable publication intent may reconcile
without republication when authoritative state already proves the exact requested
generation, or retry the exact retained request while the exact prior generation
remains authoritative. A terminal indeterminate publication receipt is stronger
evidence: if authoritative state still exposes the prior generation, pkgctl must
not discard that receipt and manufacture an automatic retry; external resolution
is required. An interrupted lifecycle intent with no terminal process evidence is
similarly external-resolution-required and commits no further durable transition.
Outer-lease loss has one additional cross-journal rule. The effect journal may
seal `outer_lease_lost` as exact controller evidence while the transaction
dispatch deliberately remains started and retains that result identity as an
ordered observation. If restart finds the terminal effect before that run
observation was committed, it commits the observation once; a bounded drive
classifies that same durable non-retiring observation as an external-resolution
stop instead of consuming another iteration. If the started dispatch already
retains the same result identity, reconciliation must stop at external resolution
rather than submit a duplicate observation. This remains
true when publication itself completed before the lease was discovered lost;
canonical state visibility does not retroactively restore controller ownership.
Target-lock contention is a different control state from lease loss. The POSIX
provider remains nonblocking and owns the `lock_busy` mechanism fact; the native
driver source translates only that fact into generic
`transaction_effect_authority_unavailable`. Fresh contention occurs after the
operation reservation is durable but before effect admission. The advancement
releases that unstarted reservation, returns
`mutation_authority_unavailable`, and records no effect result. Recovery
contention cannot release a started dispatch: it returns the same disposition
without a run/effect successor. Bounded driving stops on either form and never
waits or retries inside the call. Other lease-provider errors remain mechanism
errors. A later explicit drive is the caller's retry authority.

The existing `pkgctl run` frontend consumes the same control state without
adding policy. A privileged vertical derives the exact command mutation domain
from the read-only resolution target-binding and transaction identity, holds
that domain through `libpkgapply-posix`, and requires fresh `--start`
contention to preserve the admitted run while releasing only the unstarted operation
dispatch. The frontend must render `mutation-authority-unavailable`, reject another `--start`
for the same nonce, and perform no same-call retry. After the holder releases,
one explicit `--resume` owns retry and must reserve a different operation
dispatch; resume remains based on the retained command evidence even if live
collection bytes have disappeared. A complementary recovery vertical first leaves
one operation dispatch started at durable application intent, then holds the same
mutation domain while `--resume` reconstructs that attempt. Contention there must
return `mutation-authority-unavailable` with zero run/effect successor and preserve
the exact dispatch/effect identities. Releasing the holder gives a later explicit
`--resume` authority to continue that same started dispatch; allocating a
replacement operation would erase already-owned effect history and is forbidden.
The frontend lease-loss vertical exercises the different already-owned-authority
failure. A marked post-install lifecycle recipe is enacted by a dedicated static test
interpreter that blocks on a FIFO handshake while an external revoker unlinks the one real anchored POSIX lock. The lifecycle
process is not released until that unlink succeeds, so lease loss is guaranteed to
be observed after application/post-install completion and before publication.
`--start` must return `external-resolution-required` on the same durable operation
observation, leave the operation dispatch started with exactly one observation,
and expose a terminal `outer-lease-lost` effect. Because loss of ownership is not
contention, later `--resume` calls may not acquire a replacement lease or
operation dispatch; they preserve the same run/effect heads and append no durable
successor even after live collection bytes disappear.

This qualifies runtime and current frontend wiring before another CLI option is
allowed to become its first consumer.

## Release 0.35.0 bounded native command boundary

The final functional closure is one command, not a new semantic subsystem:

```text
--start: explicit transaction semantics + run nonce
--resume: retained transaction semantics + same nonce
             + live native roots and credentials
             + current canonical-store pathname
                         |
        immutable command evidence
        owner-encoded effect restart bodies
                         |
       native_posix_transaction_run_runtime
                         |
            at most --max-steps advances
```

Start acquires and composes exactly once, then proves that the selected native
Linux backend can establish the guarantees required by the transaction's build,
check, and lifecycle nodes. Capability absence is a control-plane refusal: no
initial command evidence or run/effect evidence is retained. Only after that
preflight does start retain the current private command-evidence format: the
complete start-only transaction inputs, exact admitted interpreter identity, exact
construction/check/lifecycle backend capability profiles, and the original owner-encoded catalog and
state snapshots.
Resume supplies no second collection, target-binding, architecture, goal,
resolution-policy, or convergence request. It reconstructs the retained request from those inputs,
binds its retained target identity to the caller-supplied current canonical-store
pathname, and recomposes the same transaction identity without collection
reacquisition or live-state replanning. Semantic start options on resume are a
usage error. Bytes outside the one current private command-evidence format fail
closed; there is no compatibility decoder or migration path. An already admitted
run is refused by start; a missing run is refused by resume.

The command envelope retains its admitted construction/check/lifecycle profiles using
the canonical libpkgexec owner encoding. Historical construction/check attempt evidence
also retains the exact libpkgexec-owned backend-profile body beside the subordinate
result encoding. Historical lifecycle execution evidence is self-contained at the
libpkgapply-exec owner boundary and carries the exact libpkgexec profile bytes that
produced its embedded execution result. Rehydration therefore does not re-observe the
old interpreter pathname, query a live backend, or inject a command-level profile as
historical decode authority. Current interpreter observation, the live backend report, and supervisor credentials are
execute-now authority: resume preflights them only for construction/check/lifecycle
scopes that can still run. Completed, failed/stopped, and already externally blocked
durable states therefore reopen without proving capabilities for actuators they cannot
invoke. Remaining executable work still requires the current interpreter identity and
profile to equal their admitted identities and the relevant explicit credentials to
match the current supervisor. Conversely, once construction has completed and only
application/publication work remains, the interpreter coordinate is outside the live
scope and must not be re-observed merely because it was part of historical execution
authority. The native composition root represents absent current
process authority as null construction/check/lifecycle backends; historical
construction/check profiles stay inside their durable attempt evidence and are never
wrapped as executable backend objects.

Construction/check execution and lifecycle execution remain separate command
authority domains. The CLI therefore accepts distinct existing root views and
distinct explicit numeric credential sets for them; choosing the same values is
a caller decision rather than controller policy. The current native Linux
backend admits only the supervisor's current credentials, so preflight refuses
a transaction whose relevant explicit credential set differs. `pkgctl` does not
claim fakeroot, logical ownership virtualization, or a credential transition
that the build/package owners do not model.

The command-private body store implements both restart-body source and durable
body sink. Each lifecycle result, application receipt, publication request, and
publication receipt is owner-encoded and immutably retained before the effect
journal records its identity. Application intent uses the direct
`libpkgapply-posix` active-request index, so recovery needs no directory scan or
controller-owned application format. Read-only reopening of private journal
locks, heads, snapshots, evidence indexes, and evidence bodies is nonblocking
before regular-file validation. A corrupted special-file slot therefore cannot
turn retained authority into an unbounded wait.

Construction and check have the same closed-history rule at the controller
boundary. Once a construction attempt starts, its exact admitted
`construction_session` is canonically retained by `pkgctl` alongside the
owner-encoded fetch and build evidence. Once a check attempt starts, its exact
admitted `transaction_check_session` is likewise retained beside the
owner-encoded check result. Restart decodes either controller-owned session
under the retained transaction/progress/node authority and never reconsults the
fresh session locator, current construction/check roots or policies, or the
retained-installed-package resource source. Those inputs remain fresh-attempt
authority; their retained admitted result is historical authority.

The live operation authority observes the target only for a fresh exact current
dispatch. Before an admitted operation session can be named by either effect or
run journal, `pkgctl` canonically retains the complete admitted operation session.
The command store also retains the exact incoming-authority-to-construction-artifact
path binding required to reopen that admitted application. Started and completed
operation recovery selects those bodies by retained session/incoming identity and
never replays the live operation specification, predecessor construction lookup, or
target observation. This remains per-dispatch authority: future reserved operations
receive fresh observations rather than a transaction-wide frozen snapshot. Runtime dependency identity is the transitive resolver run-scope
closure rooted at the acted package, not transaction SCC-cohort storage.
Application target identity and lifecycle process capability remain independent
authority domains.

The runtime hierarchy is explicit and existing-only. The command creates no
store or workspace namespace, discovers no journal, loops no more than the
positive invocation bound, retries no timer, rolls back no side effects, and
performs no repair, cleanup, compaction, or garbage collection. This closes the
functional architecture; subsequent work is qualification and publication.

## Release 0.34.0 native target/runtime composition boundary

The controller now has one stable native composition root instead of requiring
an outer caller to manually retain every intermediate adapter lifetime:

```text
sealed transaction + fixed roots/lifecycle configuration
        + run/evidence/effect namespaces
        + target-lock namespace when operations exist
        + live semantic owner sources
        + selected physical backends
                         |
       native_posix_transaction_run_runtime
                         |
      bounded launch or exact-journal drive
```

The root owns the POSIX run, construction/check evidence, and effect stores; the
native construction/check locator; native operation and archive authority; the
native completed-progress context; store-backed progress rehydration; restart
authority; construction/check/effect drivers; and canonical dispatch nonce
projection. Member lifetime follows that dependency order, so no adapter
outlives a store or authority it uses.

Composition is not semantic ownership. Retained installed-package trees, live
per-dispatch operation specifications, and subordinate effect-restart bodies
remain borrowed from their owners. Live operation specifications and observations
are fresh-dispatch authority only. Once an operation starts, its complete admitted
session and incoming archive coordinate are historical authority and are decoded
without consulting those live providers. Future operations still observe against
their exact current progress rather than a transaction-wide frozen target. Backends, credentials, roots, archive coordinates,
and state storage are explicit inputs rather than discovered policy.

Construction/check resource authority now consumes exact semantic progress
rather than the wider `transaction_run`. Caller run intent has no bearing on a
source tree, package-input resource, root view, or check workspace. The durable
run identity remains required separately where it actually owns effect-attempt
nonce and journal history. This split lets the native progress rehydrator replay
completed construction/check evidence through the same locator and backend
profile used by fresh and restarted work. Terminal operations are reopened by
the same native operation authority and canonical checkpoint validator.

Path-based construction opens three existing absolute, normalized journal
directories and, for an operation-capable transaction, the fourth target-lock
directory with final-component no-follow directory semantics, then delegates to
the corresponding descriptor-based factory. Normalized path overlap is refused
before opening; aliased descriptors are refused by device/inode identity. No
directory is created, initialized, scanned, inferred, or selected. Build/check writable roots must remain disjoint from lifecycle execution,
target, and lifecycle-session authority. Construction/check and lifecycle root
views remain independent typed authorities; when they deliberately use the same
host path, their identities must agree.

Every launch still requires an explicit durable run-intent nonce and a positive
drive bound. This release adds no CLI command, target discovery, backend
selection, scheduling, retry, waiting, worker, cleanup, compaction, or garbage
collection. The next functional boundary is the single narrow mutating command;
after that, work moves to one coordinated qualification and publication sweep.

## Release 0.33.0 native operation authority boundary

Operation authority is now one exact translation from sealed transaction and
current progress into an admitted effect session. It is not a new planner,
application model, archive catalog, target observer, or executor:

```text
sealed transaction + exact progress + reserved dispatch
             + replayable operation specification
             + fixed lifecycle configuration
                              |
                 existing preparation authorities
                              |
             exact effect request + admitted lifecycle sessions
                              |
            fresh attempt nonce or exact restart checkpoint
```

One replayable specification is requested for the exact current dispatch and
must name that action and operation kind. This avoids freezing future target
observations across state-changing actions. Incoming operations require the
exact successful predecessor construction already retained in progress. The
specification also supplies explicit lifecycle execution order. The existing
effect boundary validates that this order contains exactly the lifecycle nodes
attached to the action by transaction phase edges; deterministic graph storage
is not execution precedence. Executable lifecycle nodes come only from
`libpkgapply-exec`. When both exact lifecycle phase sets are empty, operation
authority derives no lifecycle nodes and requires no lifecycle-executor binding
from the application target. A non-empty phase set enters `libpkgapply-exec` and
therefore retains its strict target-selected executor binding. Each lifecycle
scratch path is one deterministic direct child of the caller-provisioned session
root. Its child identity is domain-separated by stable run journal, dispatch,
phase, and phase index. This preserves the no-I/O operation-authority boundary
and satisfies `libpkgapply-exec`'s contract that the leaf parent already exists;
fresh and restarted acquisition reproduce one session without touching the
filesystem.

Restart authority loads only the exact effect attempt retained by the run. The
effect journal remains control memory: lifecycle results, application receipts,
publication values, and an optional application journal are supplied by their
owner and validated by the existing checkpoint constructor. A separate explicit
archive map binds one incoming authority to one retained path and opens it under
its already admitted complete-archive digest.

This release does not observe or mutate a target, execute lifecycle or
application work, append a journal, read canonical state, select backends,
discover paths or credentials, retry, schedule, or expose a command. Target and
runtime composition remains the next boundary.

## Release 0.32.0 exact progress-rehydration boundary

Durable run records retain controller ownership and terminal identities, not a
second semantic package model. The production rehydrator reconstructs one exact
`transaction_progress` by replaying completed durable dispatches over the
sealed transaction graph:

```text
sealed transaction + completed run history
                    |
        exact typed evidence selection
          /          |          \
 construction      check      terminal effect
 owner codec       owner codec   checkpoint
          \          |          /
            graph-ordered progress replay
                    |
       exact durable progress/state identity
```

Construction and check records are selected by journal, dispatch, and attempt
identity. Their complete semantic bodies remain caller authority and are
validated before the existing canonical decoders run. Operation history must
name one exact terminal effect record; pure terminal reconstruction cannot
continue physical work or consult a target. Successful state transition uses
the public pure `libpkgstate` publication projection.

Only completed dispatches become facts. Reserved, started, released, or
nonterminal effect records remain unresolved control history. Replay follows the
transaction graph rather than journal vector order, and the final progress
identity, current-state epoch, complete flag, and failed flag must equal the
durable record. Any missing, foreign, contradictory, or unresolvable evidence
fails closed.

This release closes historical semantic reconstruction only. It adds no fresh
operation authority, archive lookup, backend composition, retry, repair, target
profile, run-intent policy, or command actuation.

## Release 0.31.0 native session/resource locator boundary

The shared session seam now has one native controller-private implementation.
It does not discover package semantics from the host. It translates already
sealed transaction authority into exact call-scoped physical coordinates:

```text
transaction + exact progress + journal + dispatch
                         |
                         v
            native session/resource locator
              /                     \
             v                       v
catalog predecessor resource   retained installed tree
             \                       /
              v                     v
          exact construction/check session
```

For a catalog construction subject, the locator matches the candidate's
collection to the exact acquisition specification retained by the transaction.
The source document must be an absolute native `recipe.yml` coordinate directly
beneath that collection root. The source origin remains diagnostic provenance;
only its agreement with the explicit acquisition authority permits the physical
path projection.

Each catalog-selected build input must have exactly one successful predecessor
construction in the current progress. The locator reuses that construction's
exact package-output resource identity and path. Each installed-selected input
is requested from a caller-owned `retained_installed_package_tree_source`; the
returned package identity must equal the selected installed authority. No
package tree is reconstructed from release, source, artifact, or digest values.

Writable paths are derived beneath explicit disjoint configured roots from the
stable run-journal and dispatch identities. The journal, rather than the current
record identity, scopes the path so a reserved dispatch and its started restart
checkpoint reproduce one session. The locator does not inspect path existence,
create or remove directories, materialize source bytes, select or construct a
backend, execute work, read or write a journal, or advance progress.

Check realization consumes the successful predecessor construction. It reuses
its exact source-tree, package-output, and package-input resource identities.
The staged source host path comes from
`libpkgbuild-exec::project_prepared_paths()`; `pkgctl` does not own the
adapter's `source`/`work`/`tmp` vocabulary. Check admission seals the canonical
execution request through the pure adapter projection.

This is one bounded provider, not a final target profile and not a new library.
The next controller closure is a concrete
`transaction_progress_rehydration_source` over the exact transaction, canonical
state, construction/check evidence, and effect journals. Operation authority,
archive lookup, final backend composition, and a mutating command remain later
steps in that order.

## Release 0.30.0 shared session and pure recovery projection boundary

Fresh construction/check execution and restart recovery must not receive
independent context providers. The runtime now borrows one deterministic
`transaction_dispatch_session_source` and composes it in both directions:

```text
                    durable record + dispatch
                              |
                              v
             deterministic construction/check session
                       /                 \
                      v                   v
             fresh execution        restart recovery
                      |                   |
              effectful prepare     rematerialize source
                                          +
                                  pure request projection
                                          +
                                  selected backend profile
```

`libpkgbuild-exec` and `libpkgcheck-exec` expose pure execution-request
projections. They bind the exact logical resources, root view, program,
environment, credentials, limits, and cancellation policy without opening,
creating, removing, staging, or chmodding any path. Their existing `prepare()`
functions remain the effect boundary and use the same canonical request.

For construction recovery, the native context source reacquires the exact
`libpkgfetch` materialization from the session's retained request and roots,
admits the build-exec session, reproduces its execution request, and reads the
capability profile from the selected construction backend. Check recovery
reproduces the request from the exact admitted check session and selected check
backend. The evidence-backed decoder still validates every retained identity.

Operation execution and recovery remain separate sources because operation
restart belongs to the effect-journal boundary. The runtime composes an
operation-only execution source with the shared construction/check session
source; it does not widen the evidence store into operation authority.

This release does not implement the session source. Concrete resource, path,
predecessor artifact, retained installed-package, root-view, credential, and
workspace policy remains caller-owned. The next native composition must realize
that policy without reconstructing semantic authorities from paths.

## Release 0.29.0 evidence-backed semantic recovery boundary

Release 0.29.0 composes the durable construction/check evidence store with the
existing subordinate decoders and controller recovery handoff:

```text
started durable dispatch
          |
          v
exact journal / dispatch / attempt index
          |
          v
canonical subordinate result bytes
          +
caller-owned original semantic context bodies
          |
          v
validate every retained identity
          |
          v
libpkgbuild-exec or libpkgcheck-exec canonical decoder
          |
          v
recompute exact pkgctl controller-result identity
          |
          v
transaction dispatch recovery handoff
```

The store-backed source owns selection and validation, not context discovery.
For construction, the caller supplies the exact admitted construction session,
genuine source materialization, execution request, and backend capability
profile. For check, it supplies the exact admitted check session, execution
request, and backend profile. The durable record already binds the identities
of those bodies; the source refuses any mismatch before decoding.

The subordinate decoder receives complete original authorities rather than
identity-shaped substitutes. Its decoded execution, request, backend, build or
check result, and controller request are checked again against the durable
record. The controller then reconstructs the private result body only through
the same canonical identity domain used during fresh execution. A different
identity is corruption or foreign context, not a recoverable variation.

An absent evidence record means a started dispatch lacks recoverable execution
evidence. It is not interpreted as a never-started reservation and does not
release ownership. Operation recovery remains delegated to the effect-journal
boundary because construction/check evidence storage does not own lifecycle,
application, publication, or target-observation records.

`posix_transaction_run_runtime` now owns this store-backed recovery composition.
It borrows a context source rather than a caller-created construction/check
result source. The caller is still responsible for locating and realizing the
original sessions, source objects, execution requests, backend profiles, and
concrete resources. This release performs no context discovery, process
adoption, retry selection, scheduling, cleanup, or frontend mutation.

## Release 0.28.0 durable construction/check evidence boundary

Release 0.28.0 closes the physical durability gap between successful
construction/check execution and terminal transaction-run retirement.

```text
committed reserved dispatch
            |
            v
commit exact started run successor
            |
            v
execute admitted construction/check session
            |
            v
admit typed dispatch evidence record
            |
            v
persist immutable canonical result bytes
            |
            v
persist immutable journal/dispatch/attempt index
            |
            v
commit terminal run successor
```

The evidence record binds the exact run journal, transaction, dispatch, graph
node, attempt session, controller result, original controller request, and every
subordinate request/backend/execution/result identity needed to validate later
rehydration. Construction additionally retains a canonical controller-owned
encoding of its exact admitted `construction_session`, the canonical
materialization encoding owned by `libpkgfetch`, and the canonical
build-execution encoding. Check retains the canonical check-execution encoding.
`pkgctl` does not invent a second serialization for any subordinate body.

The POSIX store publishes the content object before the typed index and
synchronizes both directory transitions. Exact retries are idempotent. A
conflicting record for one journal/dispatch/attempt, a corrupt index, a missing
indexed object, or altered content fails closed. Descriptor anchoring preserves
the selected store authority if its original pathname is renamed or replaced.

This closes durable observation, not semantic resurrection. Subordinate codecs
require complete caller-owned semantic authorities. Construction recovery
supplies the exact retained source snapshot to `libpkgfetch`, which decodes the
retained historical materialization body without reopening source locators or
content-store objects; build/check recovery likewise delegates their retained
bytes to their owners. The store deliberately does not promote an identity into
a semantic result or search the host for missing authority. Transaction-run
evidence schema 1 is the only admitted private format; incompatible development
bytes fail closed and have no reconstruction path.

`posix_transaction_run_runtime` now retains four caller-opened directory
authorities: a transaction-run journal directory, a
construction/check evidence directory, an effect-attempt journal directory,
and a target-mutation lock directory. The runtime still performs
no discovery, store initialization, semantic rehydration, resource realization,
retry, scheduling, cleanup, or command action.

## Release 0.27.0 resolver-backed construction authority

Release 0.27.0 removes the controller's last caller-written imitations of
resolver, build-materialization, and image authority.

```text
transaction resolution + selected build node
                    |
                    v
        pkgbuild::build_request
                    |
      exact logical build/check inputs
                    |
                    + caller-scoped resource identities and paths
                    v
          build/check execution session
```

A construction request seals its `libpkgbuild` request directly from the exact
resolution retained by the transaction. The request's build/check inputs are
therefore the resolver-issued direct requirement edges and selections. Callers
cannot supply release, source, build-result, artifact, or invented tree
digests. A construction or check session admits only one concrete resource
identity and host path for every exact logical input.

Construction retains the complete successful `libpkgbuild-image` admission
returned by `libpkgbuild-exec`. Operation preparation consumes that authority
through the standalone pure `libpkgbuild-plan` projection. It does not reopen,
size, or inspect archive bytes and does not reproduce payload/image equality.

The effect journal has one first-generation encoding. Immutable snapshots are
committed only by their checksummed durable head; an orphan snapshot is not a
legacy history and fails closed. This correction adds no materializer,
discovery policy, scheduler, retry loop, or mutating command.

## Release 0.26.0 explicit run intent and canonical dispatch nonce boundary

Release 0.26.0 separates two values that were previously hidden behind the same
abstract source shape:

```text
caller intent                         committed controller head
     |                                         |
explicit run nonce                    journal + record + reopened run
     |                                         |
     v                                         v
select durable history       canonical dispatch nonce derivation
```

The transaction-run nonce distinguishes caller intent. Two otherwise identical
initial runs can intentionally name different durable histories, so the
controller cannot derive that nonce from transaction semantics without erasing
that distinction. `posix_transaction_run_runtime::launch()` therefore receives
one explicit `transaction_run_nonce`. Exact launch retry is the caller's visible
act of supplying the same intent nonce again.

A dispatch nonce has different semantics. It distinguishes fresh ownership
attempts inside one already-selected run history. The committed journal head is
already the exact retry domain: failure before reservation commit leaves that
head unchanged, while any committed reservation, release, observation, or
completion creates a successor. `canonical_transaction_dispatch_nonce()` first
proves that the provided run is the exact reopening of the committed record and
then derives a domain-separated SHA-256 value from the journal, record, and run
identities. The same head yields the same nonce; a successor yields another.

The POSIX transaction-run runtime owns one stateless canonical dispatch source.
It no longer borrows run or dispatch nonce services. This introduces no nonce
store, random generator, seed, hidden cache, journal enumeration, or process
lifetime dependency. Run-intent generation and persistence remain outside the
runtime; semantic progression, dispatch authority, archive, backend, and state
authorities remain borrowed exactly as before.

This boundary adds no scheduler, retry loop, worker, cleanup, discovery,
semantic reconstruction, or mutating command.

## Release 0.25.0 native transaction-run runtime boundary

Release 0.25.0 composes the already-qualified run and effect mechanisms into
one caller-configured lifetime boundary:

```text
caller-opened run, effect, and target-lock directories
                         +
caller-owned nonce, semantic, archive, backend, and state authorities
                         |
                         v
          posix_transaction_run_runtime
                         |
          +--------------+---------------+
          |                              |
 exact progress + policy           exact journal identity
          |                              |
       launch()                        drive()
          |                              |
          +---------- bounded control ---+
```

The runtime owns the POSIX transaction-run store, POSIX effect-attempt store,
native construction driver, native check driver, and one
`posix_transaction_effect_driver_source`. The three underlying
descriptor-owning mechanisms duplicate the caller-selected directory
descriptors.
Renaming or replacing a pathname after construction cannot redirect the retained
authority. Destruction closes the retained descriptors and destroys no store
content.

All policy-bearing authority remains outside. The runtime borrows the replay-safe
run and dispatch nonce sources, semantic progression rehydration, exact
execution and recovery authority sources, archive source, construction/check
execution backends, application and lifecycle backends, and canonical state
store. These objects must outlive the runtime. It neither derives them from
durable identities nor stores them in either journal.

`launch()` delegates to `launch_transaction_run()` with one caller-supplied
progression, dispatch policy, and positive drive bound. `drive()` delegates to
`drive_transaction_run()` for one exact journal identity and positive bound.
Both methods preserve the existing write-ahead reservation, per-dispatch
authority, and subordinate effect barriers. The runtime does not add another
state machine around them.

This boundary performs no path or journal discovery, store initialization,
nonce generation policy, semantic rehydration, archive or backend selection,
credential selection, waiting, retry loop, scheduling, worker creation,
cleanup, compaction, repair, or frontend action. It is a composition root for
callers, not yet a package-operation command.

## Release 0.24.0 native effect-runtime boundary

Release 0.24.0 implements the abstract per-dispatch source without moving
selection policy into the controller:

```text
caller-selected backends, canonical store, archive source, lock directory
                                |
                                v
                   exact semantic dispatch handoff
                                |
                 archive admission before target lock
                                |
                                v
              fresh nonblocking POSIX outer target lease
                                |
              +-----------------+------------------+
              |                                    |
 lease-bound canonical projection          target-scoped read
              |                                    |
              +----------- exact authority shape--+
```

The source is one configured target-runtime mechanism, not a global service
locator. It duplicates the caller-opened lock-directory descriptor but borrows
the selected application backend, lifecycle execution backend, canonical state
store, and archive source. Those borrowed authorities must outlive the source
and every acquisition call. No path, credential, backend, archive, store, or
policy is inferred from a durable identity.

`acquire_transaction_effect_archive()` validates replay authority before target
locking. An incoming request must receive one archive whose package-image and
inspection-receipt identities exactly match the admitted incoming authority. A
removal request receives no archive and does not call the source. Archive
selection remains caller authority; pkgctl validates only the returned fact.

For continuation, the source acquires a fresh POSIX outer lease and calls
`pkgstate::apply_adapter::read_application_state()`. That adapter performs the
canonical read under the live lease and derives the exact projection evidence.
The resulting continuation driver and state observer share one owned runtime,
so destroying either object cannot release the lease while the other still
exists. Any archive, lease, or state-admission failure unwinds without retaining
physical authority.

Recovery remains classifier-driven. Continuation checkpoints receive the same
lease-bound continuation/observer pair. Terminal success receives a new
lease-bound state observer without an obsolete application projection.
Publication recovery receives a target-scoped publication driver and no archive
or lifecycle/application authority. Terminal failure and external-resolution
checkpoints acquire nothing.

This boundary composes existing mechanisms; it does not initialize stores,
construct backends, choose credentials, enumerate archives, discover lock
locations, wait for locks, retry, schedule, append journals, clean up history, or
expose a mutating frontend command.

## Release 0.23.0 split effect-authority boundary

Release 0.23.0 separates three physical capabilities that the previous
per-dispatch driver surface combined:

```text
continuation authority       resulting-state observation
  lease + old projection       lease + canonical read
           |                            |
           +------------+---------------+
                        |
              successful run evidence

publication reconciliation
  lease + canonical read + exact retained publication retry
```

A lease-bound application projection describes the state from which an
application was authorized. It remains valid for lifecycle execution,
application, and ordinary publication in that effect attempt. It is not a
truthful prerequisite for observing state after publication may already have
advanced the canonical store. Recovery must therefore not reconstruct or
fabricate that old projection merely to decide whether a retained publication
request already took effect.

`transaction_effect_driver` is now continuation authority only.
`transaction_effect_state_observer` can read canonical state under one
caller-owned target-scoped lease. `transaction_effect_publication_driver`
extends that observer with retry authority for the exact durable publication
request. Fresh operation advancement acquires continuation and observation as
one call-scoped bundle and proves both use the same lease acquisition. Recovery
acquires exactly the subset selected by the pure restart classification.

The controller rejects missing, surplus, foreign-target, expired, or
cross-acquisition authority before effect continuation. Terminal failure and
external-resolution states acquire nothing. A terminal success receives only a
state observer. Publication-intent and indeterminate-publication recovery
receive only publication authority. Continuation paths receive the old-state
driver plus a distinct observer because they may complete successfully.

This boundary adds no native driver-source implementation, path or credential
discovery, lease acquisition, state-store selection, archive selection,
backend construction, scheduler, worker, retry policy, cleanup, or mutating
command.

## Release 0.22.0 per-dispatch effect-driver authority boundary

Release 0.22.0 removes the last global physical-operation driver from bounded
transaction-run advancement:

```text
committed operation reservation
              |
              v
 exact execution or recovery handoff
              |
              v
 caller-owned effect-driver source
              |
              v
 one call-scoped operation driver
```

A transaction may contain more than one target operation. Each operation can
require a different target mutation lease, installed-state projection, incoming
archive, lifecycle backend, application backend, and recovery binding. A single
driver supplied for the whole run could therefore carry authority for the wrong
dispatch. `transaction_effect_driver_source` instead receives the exact
validated dispatch handoff and returns one uniquely owned driver for that
advancement call. The driver is neither stored in the run journal nor reused for
a later operation.

Fresh acquisition occurs only after the reservation successor is durable and
after semantic execution authority has been admitted. Before the effect attempt
may be admitted, the controller validates the returned driver's live target
mutation lease and proves that its state projection names the exact expected
snapshot and ownership inventory sealed into the operation session. Source
refusal or invalid physical authority leaves the durable dispatch reserved and
creates no effect-attempt record.

Recovery first classifies the exact retained effect checkpoint. Physical
authority is requested only when continuation can touch the target or when a
successful terminal effect requires a resulting-state read for run progression.
Terminal failure and external-resolution states request no driver. A driver
source therefore cannot turn unresolved evidence into target access merely
because a run is being inspected for restart.

This boundary adds no concrete lease, state-store, archive, backend, credential,
or path discovery. It adds no driver serialization, driver retention, scheduler,
worker, concurrency, retry policy, cleanup, reconciliation policy, or mutating
command. Native runtime assembly remains a caller-owned composition problem for
a later release.

## Release 0.21.0 exact effect-inspection command boundary

Release 0.21.0 exposes the existing durable effect sensor through one exact
read-only command:

```text
explicit effect-store path + exact attempt identity
                       |
                       v
         open existing POSIX effect store
                       |
                       v
             inspect one committed head
                       |
                       v
          deterministic inspection report
```

`pkgctl inspect-effect` accepts both coordinates explicitly. It neither scans
the store nor derives an attempt from a transaction-run journal. The command
opens an existing caller-owned POSIX store, calls `inspect_effect_attempt()`,
and emits the already-defined deterministic report. Invalid attempt syntax is a
usage error; store access, missing-head, corruption, and storage-authority
failures retain typed effect-journal diagnostics.

The command inherits the sensor's non-mutating POSIX reader protocol. An
existing writer lock is opened read-only and acquired shared; an absent lock is
not created, and a concurrent writer establishment is detected and rechecked.
The frontend therefore requires no write authority and performs no store
initialization.

The command performs no attempt enumeration, latest-attempt selection,
run-journal traversal, semantic evidence rehydration, restart checkpoint
construction, driver invocation, append, reconciliation, repair, scheduling, or
mutation. It is a frontend for the qualified sensor, not a second effect
controller.

## Release 0.20.0 durable effect-attempt inspection boundary

Release 0.20.0 adds the read-only sensor paired with the durable effect
actuator:

```text
caller-selected effect-attempt identity
                  |
                  v
      storage-derived committed head
                  |
                  v
        pure effect restart assessment
                  |
                  v
       deterministic evidence report
```

`inspect_effect_attempt()` loads exactly one committed head and rejects absence
or a store response naming another attempt. It returns the storage-derived
`effect_attempt_record` together with `assess_effect_restart()` output. The
classifier therefore remains the same pure authority used before executable
restart; inspection cannot drift into an alternative stage or continuation
policy. Terminal and automatically continuable are separate facts: a terminal
effect journal is automatically consumable by run reconciliation even though no
more subordinate effect should execute.

The report is deterministic and line-oriented. It exposes the exact attempt,
record, session, nonce, predecessor, sequence, stage, restart disposition,
lifecycle completion identities, application receipt and journal identities,
transaction evidence, publication request and receipt identities, terminal
outcome, and reconciled state retained by the controller. Optional fields remain
absent when the record does not retain them. These strings remain identities;
inspection does not reconstruct lifecycle results, application receipts or
journals, transaction semantics, publication values, or installed-state
snapshots.

The POSIX effect store now separates reader and writer lock authority. Existing
locks are opened read-only and acquired shared. When no lock exists, the reader
performs one unlocked observation and rechecks for a concurrently established
writer lock before accepting or propagating that observation. Only append may
create the lock, open it read-write, or acquire it exclusively. Empty-store and
inspection reads therefore create and modify nothing.

This release adds no CLI, attempt enumeration, latest-attempt selection,
run-journal traversal, semantic rehydration, restart checkpoint construction,
driver invocation, append, reconciliation, repair, scheduling, or mutation.

## Release 0.19.0 exact run-inspection command boundary

Release 0.19.0 exposes the existing durable sensor through one exact read-only
command:

```text
explicit run-store path + exact journal identity
                    |
                    v
      open existing POSIX run store
                    |
                    v
          inspect one committed head
                    |
                    v
       deterministic inspection report
```

`pkgctl inspect-run` accepts both coordinates explicitly. It neither scans the
store nor selects a latest or convenient run. The command opens an existing
caller-owned POSIX store, calls `inspect_transaction_run()`, and emits the
already-defined deterministic report. Invalid command identities are usage
errors; store access, missing-head, corruption, and store-authority failures
retain typed journal diagnostics.

Read-only POSIX loads acquire the existing writer lock only through a shared
read-only descriptor. An empty caller-created store is inspected without
creating a lock file, and a concurrent writer is detected and retried under the
newly established lock. The command therefore does not require store write
permission and does not initialize or alter the store.

The command performs no semantic progression rehydration, journal enumeration,
effect-journal access, append, reservation, execution, repair, scheduling, or
mutation. It is a frontend for the existing sensor, not a second inspection
implementation.

## Release 0.18.0 durable transaction-run inspection boundary

Release 0.18.0 adds the read-only sensor paired with the durable run actuator:

```text
caller-selected journal identity
        |
        v
storage-derived committed head
        |
        v
controller-owned restart assessment
        |
        v
deterministic report
```

The inspection boundary loads exactly one journal head and rejects a missing head
or a store response naming another journal. It classifies the durable record as
completed, stopped after terminal failure, active, or quiescent incomplete. The
active assessment is derived only from retained dispatch state, attempt identity,
effect-attempt identity, and uncertainty observations.

The pure `assess_transaction_run_record()` function is shared with semantic
restart checkpoints so the read-only view and executable recovery path cannot
drift into different ownership classifications. Rehydrated progression remains
required before a run may be executed; inspection never fabricates that
progression from journal identities.

The report is deterministic and line-oriented. It exposes the exact journal,
record, sequence, transaction, run, progress, current-state and dispatch-policy
identities, every retained dispatch, and every active restart disposition. This
layer does not scan for journals, append records, reserve or execute work, inspect
subordinate effect journals, discover semantic evidence, or expose a command
action.

## Release 0.17.0 restart-safe transaction-launch boundary

Release 0.17.0 composes durable admission with bounded serial driving while
remaining exactly retryable after the journal has advanced:

```text
immutable initial run
        |
        v
replay-safe run nonce
        |
        v
expected journal identity
        |
        +-- no head --> commit sequence zero
        |
        `-- head ----> validate exact admission universe
                         |
                         v
                 bounded serial drive
```

The run nonce is requested before storage so the expected journal identity is
known without scanning. The controller loads only that exact journal. No head
permits one sequence-zero append through the existing admission commit helper.
A returned head is accepted only when its journal, transaction, nonce, and
dispatch-policy authority match the expected initial admission universe. A
sequence-zero head must be byte-semantically identical to the expected
admission record.

This distinction is required because raw admission is not a rewind operation.
Once successor records exist, republishing sequence zero would violate append
order. An exact launch retry instead resumes the storage-derived current head and
lets the bounded drive reconcile retained ownership before fresh work. A
completed head returns through the drive's quiescent completion path without a
new append or driver invocation.

The launch result retains the storage-derived starting record, whether this call
committed admission, and the complete bounded-drive result. It validates that
the drive remains in the same journal, transaction, nonce, and dispatch-policy
universe and never moves behind its starting record.

This layer does not discover journals, derive nonces, create stores or drivers,
run without an explicit bound, create workers or concurrency, choose retry
timing or adaptive priority, discover resources or evidence, adopt processes,
roll back, clean up, compact history, collect garbage, or expose a mutating
command.

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

Fresh `executed_operation` advancement evidence retains that admission record as
the cross-journal write-ahead coordinate even when execution subsequently seals
a later effect record. It is not an alias for the current effect-journal head.
Restart reconciliation, by contrast, returns the effect record selected or
produced by recovery. Code that needs the terminal head of a freshly executed
attempt must load it from the effect store by the admission's attempt identity.

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
and reproduce its source and build provenance. `libpkgtransaction >= 4.0.0`
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

## Historical release 0.6.0 operation-preparation boundary

Release 0.6.0 introduced preparation for one exact target action already
present in a sealed transaction program. It accepted only `install`, `upgrade`,
or `remove`. The caller supplied:

1. the exact `transaction_session` and target-action node;
2. one matching completed `construction_result` for install or upgrade;
3. one caller-authoritative application target and execution-control snapshot;
4. complete target observations and normalized package policy;
5. one resolved runtime-closure identity for incoming operations;
6. one complete lifecycle order and, for installation, an installation reason;
7. one injected read-only artifact-projection driver.

The original 0.6 mechanism projected the installed snapshot through
`libpkgstate-plan`, then used an injected artifact driver to reopen and inspect
package bytes before planner projection. That mechanism is historical. Release
0.27.0 removed controller artifact I/O: incoming preparation now consumes the
complete `libpkgbuild-image` authority retained by construction and passes it to
the pure `libpkgbuild-plan` adapter.

Current preparation requires the projection to retain the exact build result,
archive digest, normalized image identity, entry count, artifact identity, and
manifest identity already established by construction. It selects no inspection
backend and creates no replacement receipt.

The package-local sequence is:

```text
validate transaction action and construction binding
        ↓
project complete installed truth through libpkgstate-plan
        ↓
project retained build/image authority through libpkgbuild-plan
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
ownership universe, exact retained build/image admission, planner artifact
projection, incoming application authority, and either the planner refusal or
the complete plan, application, and effect identities.

### Read-only boundary

Preparation performs no target mutation and reads no artifact bytes. Incoming
preparation consumes the exact build/image admission retained by construction
and delegates pure planner projection to `libpkgbuild-plan`. It does not acquire
the target lease, admit executable lifecycle sessions, call an application
backend, or publish installed state. It adds no scheduler, recursive
construction, check execution, durable preparation journal, or command frontend.

## Historical release 0.5.0 construction boundary

Release 0.5.0 introduced one backend-neutral construction session for one
exact catalog-backed `build` node. Its original API accepted caller-written
package-input subjects, tree identities, and a separately assembled build
request. Those values were removed in 0.27.0 because they duplicated resolver
authority and claimed a package-tree authority that no production component
issued.

The current request is sealed directly from the transaction's exact
`libpkgresolve` result and selected build node. The caller supplies only one
closed build policy and bounded acquisition policy. The admitted session then
adds explicit local-source, content-store, root-view, workspace, output, and
artifact coordinates; one concrete `pkgexec::resource_identity` and host path
for every resolver-backed logical build/check input; interpreter identity;
numeric credentials; and one injected backend-neutral construction driver.

Admission proves that the selected node is a catalog-authorized build node, its
candidate source and release agree with resolver authority, and every concrete
package-input resource matches exactly one logical input already sealed by the
build request. Missing, duplicate, aliased, and undeclared resources are
rejected. Call-scoped paths are normalized and checked for unsafe overlap before
source acquisition begins.

The sequence is:

```text
seal the build request from transaction resolution and caller policy
        ↓
validate call-scoped construction resources
        ↓
materialize the exact source snapshot through libpkgfetch
        ↓
admit and execute one libpkgbuild-exec session
        ↓
retain verified materialization, execution evidence, build result,
and the complete libpkgbuild-image admission
```

The controller promotes `completed` only when the build result succeeded and
the complete build/image admission is retained. A backend or adapter
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
- completed application evidence permits continuation into post lifecycle and
  the receipt-named historical application journal is not restart authority;
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

A `construction_request` retains one exact transaction build node, the
resolver-backed `libpkgbuild` request, and source-acquisition bounds. A
`construction_session` adds explicit source/store and build coordinates, one
call-scoped resource identity and host path for each exact logical input,
interpreter, credentials, and compression. Concrete paths participate in the
session identity but not in the logical build-request identity.

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

Catalog acquisition deliberately preserves `pkgsource::yaml::yaml_error`. The
command frontend names that adapter-owned type directly so it can render source
location and protocol path. The controller core neither includes nor links the
YAML adapter; syntax parsing remains outside controller authority.

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
