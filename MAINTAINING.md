# Maintaining pkgctl

## Authority review

Before accepting a feature, identify which component owns every consumed or
produced fact. Reject changes that make `pkgctl` parse, infer, recompute, or
serialize a value already owned by another library.

Controller-owned policy must remain visibly separate from authority results.
Defaults that can remove packages, mutate filesystems, initialize state, or
publish state are prohibited.

## Pre-frontend vertical qualification

A new effect-implying frontend must not be the first integration test of an
already-library-owned package lifecycle. Before extending the command surface,
compose the existing controller core in-process against disposable collection,
state, content, build, package-output, artifact, application-store, lock, and
target roots. Keep acquisition, resolution, transaction ordering, fetching,
build admission, package/image sealing, check admission, progression, operation
preparation, POSIX application, effect journaling, and canonical publication
real. Replace only the external process actuator when determinism or privilege
requires it, and make that fake consume the exact resources prepared by the real
construction/check adapters.

When a dependent build is under test, obtain its concrete package-input resource
through the native session locator from successful predecessor construction
evidence. Do not manufacture a semantically equivalent directory or bypass the
transaction graph in the fixture. A green CLI is evidence about the frontend; it
must not be used as the primary proof that lower package authority composes.
The same rule applies after construction: reopen the exact retained artifact,
hold the real target mutation lease, publish the exact completed application
through the canonical store, and retire the operation against the published
snapshot. Do not replace application or state publication with semantically
equivalent controller fixtures merely to keep the campaign cheap.

Fresh target observation used by operation planning is controller-core authority,
not frontend policy. `pkgctl-core` owns conversion of complete
`libpkgapply` path facts into `libpkgplan` observations and the stable
`pkgctl/native-target-observations/1` identity domain. A frontend may retain or
replay those observations, but it must not own another conversion table. The
pre-frontend campaign must consume the same core function against its disposable
target.

The pre-frontend campaign must also cross the stable native runtime composition
root. Do not qualify package effects only by manually wiring the lower dispatch
and effect functions and then assume the frontend runtime is equivalent. A
bounded native runtime launch must reach a real operation, retain its exact
per-dispatch specification before mutation, and complete through canonical state
publication. Reopening that completed journal must obtain the retained operation
observations rather than re-observe the already-mutated target, load the exact
subordinate terminal bodies through the caller-owned restart source, and remain
quiescent without new effect authority. The runtime owns wiring and mechanical
lifetime; retention/replay remains caller authority.

Fault qualification must cross durable owner boundaries rather than inject a
controller shortcut. Definitive build and check failures must be produced by the
injected execution backend and committed through ordinary construction/check
completion so transaction progression owns which dependent work becomes blocked.
The same failures must cross the stable native runtime composition root: reopen the
failed journal through a newly constructed runtime, prove the stopped result is
rehydrated from durable evidence, and prove no failed actuator is rerun and no
operation specification, archive, effect-body, target, or state-publication
authority is consulted. Definitive operation failures must likewise enter through
the owning subordinate protocol rather than a controller shortcut: physical
application failure through `libpkgapply`, lifecycle failure through `libpkgexec`,
and canonical publication failure through `libpkgstate`. Lifecycle fault cases
must obtain their phase nodes from explicit lifecycle-scoped resolution goals; a
recipe lifecycle program is not implicit transaction authority. Reopening those
terminal failures must neither retry them nor roll them back. Preserve the
physical/canonical asymmetry instead: application or pre-lifecycle failure leaves the target
unchanged, whereas post-lifecycle or publication failure may leave a completed
application on the target while canonical state remains at its previous
generation. The same terminal effect identity and retained subordinate bodies
must rehydrate without another actuator, archive acquisition, application, or
publication call. Lifecycle session coordinates are not an excuse to move POSIX
preparation into operation authority: the configured session root is the
caller-provisioned parent, and each admitted lifecycle scratch leaf must be a
deterministic direct child of it. `libpkgapply-exec` owns creation and protection
of that single-use leaf.

Keep uncertainty distinct from definitive failure in the same runtime campaign.
A durable publication intent has not yet retained terminal publication evidence:
authoritative state may reconcile it without republishing when the exact result
is already visible, or retry the exact retained request when the exact prior
state is still current. A terminal indeterminate publication receipt is not the
same state. If authoritative state still exposes the prior generation, preserve
the receipt and require external resolution; do not discard terminal uncertainty
and turn it into automatic retry policy. The uncertainty matrix must exercise all
three publication states. A lifecycle intent with no terminal subordinate
evidence also requires external resolution. Construct that crash point at the
durable controller/effect boundary; do not make a `libpkgexec` execution backend
throw, because a throwing backend is itself a backend-contract violation.
Reopening either externally blocked state must append no run/effect successor and
must not acquire archive or physical execution authority. Repeated resume without
new evidence must remain the same externally blocked record.

Treat outer-lease loss as a cross-journal observation, not a retry signal. The
effect journal can already be terminal while the transaction dispatch remains
started because lease loss does not retire package truth. If that exact effect
identity is not yet retained by the run journal, restart may retain it once; the
bounded drive must classify that newly durable non-retiring observation as an
external-resolution stop immediately. If it is already present in the dispatch
observation list, return external resolution without submitting it again.
Do not confuse the fresh run-advance operation record with the effect-journal
head: fresh execution returns the exact write-ahead admission record by
contract. Inspect terminal lease-loss durability by loading the latest effect
record for that attempt. Likewise, do not require semantic replay during the
fresh drive once the non-retiring observation itself is the stopping step;
replay belongs to explicit restart reconstruction. Test the physical mechanism
by unlinking the real POSIX lock file; do not add a test-only lease-release
method. A publication that became visible before lease loss remains visible
state, but pkgctl still has no authority to call the operation completed
automatically.

Treat target-lock contention separately from both outer-lease loss and external
resolution. `libpkgapply-posix` owns the nonblocking `lock_busy` fact; only the
native physical source may translate that expected mechanism result into
`transaction_effect_authority_unavailable`. Fresh contention must release the
unstarted reservation before returning `mutation_authority_unavailable` and
must not fabricate an effect attempt. Recovery contention must preserve the
started dispatch and exact effect head and commit no successor. Do not catch
other lease errors as contention, and do not add sleep, waiting, backoff, or an
implicit retry loop. Retry authority belongs to a later explicit drive call.
The privileged CLI contention vertical must use the real POSIX provider, not
`flock(1)`, an errno shim, or a controller test hook. Derive the exact command
mutation domain from the same transaction identity, target-binding identity,
target root, runtime root, provider-version field, and target-lock root used by
`command_application_target()`. A blocked fresh `--start` must leave an admitted
run with a released-unstarted operation and no effect attempt; duplicate
`--start` remains an admission error. Release the external holder explicitly and
resume the same nonce. The replacement operation dispatch must differ from the
released one, and deleting the collection before resume must not change the
outcome. The recovery-side CLI vertical must start from a real durable
application-intent effect, not a synthetic started-dispatch marker. Acquire the same
real POSIX domain only after that interruption. A contended `--resume` must commit
zero run/effect successors and preserve the exact started dispatch/effect identities;
do not release or replace already-started work. Remove the collection before resume
to prove retained command authority. Once the holder is explicitly released, the
next `--resume` must complete the same dispatch and effect attempt.

For the CLI lease-loss vertical, do not approximate revocation with contention or
by terminating the controller. Synchronize a real post-install lifecycle process
with an external revoker: the process must remain blocked until the revoker has
unlinked the single anchored POSIX lock file, so pkgctl observes loss only after
that lifecycle result succeeds. Inspect the terminal effect head for
`outer-lease-lost` and the run dispatch for the one non-retiring observation.
Do not project the run-level external-resolution classification back into
`inspect-effect`: a terminal effect head is effect-locally `terminal`,
automatically continuable, and not external-resolution-required because restart
can consume that durable terminal evidence. The run/effect join is what converts
its non-retiring outcome into the controller-level external-resolution stop.
Application/post-install target effects are expected to remain while canonical
state remains prior because publication was never entered. Once that observation
is retained, later `--resume` is external-resolution-only: it may reconstruct
semantic authority to explain the block, but it must not append a durable run/effect
successor, reserve replacement work, reacquire a mutation lease, or recreate the
missing lock pathname. Delete the live collection before resume to keep the
retained-command-universe guarantee in the same proof.

A resumed command must not carry a second semantic transaction request merely so
the CLI can prove equality with itself. Command-evidence schema v3 retains every
start-only transaction input, the exact admitted construction/check/lifecycle backend
capability profiles, and owner-encoded catalog/state snapshots before run admission. Every process-level resume test must therefore
omit catalog acquisition, target-binding identities, architecture, goals,
resolution preference, and convergence policy; explicit re-declaration is a
usage error. The canonical-store pathname remains live physical authority on
resume, but its target binding comes from retained state evidence. Do not add a
v1/v2 compatibility decoder: older command evidence is not a public state authority
and must fail closed. Historical execution evidence must be decoded against the
retained profile, never a freshly probed substitute. Treat current capability reports
and current-supervisor credential equality as execute-now authority: require them only
for scopes that durable progress/recovery can still invoke. A completed or externally
blocked run must not be made unrecoverable merely because an unused actuator is no
longer executable in the current process context. Keep both sides process-qualified:
remaining check work must reject a changed supervisor, while completed and retained
outer-lease-loss runs must reopen under that changed supervisor without journal
advancement. The privileged credential-context fixture must perform the uid/gid
transition only after the dynamic loader has mapped the build-tree CLI dependency
closure. A pre-exec credential drop accidentally tests pathname accessibility of the
developer build root and is not valid execution-authority qualification. Do not break
sanitizer startup to reach that boundary: if dynamic AddressSanitizer is active, its
already-loaded runtime must remain first, and the credential hook must not discard or
precede that sanitizer runtime when composing the temporary preload chain.

Application and publication interruption must occur by
refusing an exact effect journal append after the subordinate side effect has
reached the selected durable boundary. Restart must reopen retained
run/effect/application authority and enter the production reconciliation path.
When the subordinate application journal is already terminal and the exact receipt
body was durably retained before the lost controller append, tests must prove that
controller authority adopts that terminal receipt without either a fresh
application or an application resume. When only a resumable subordinate journal
exists, the ordinary resume path remains required. Already-selected canonical
state must be observed rather than published a second time.

Qualification may directly compose a downstream library family that `pkgctl`
does not yet own in production, but that seam must remain test-only until the
controller has a real orchestration responsibility for it. In particular,
reconciliation projection and POSIX inventory persistence may qualify completed
application evidence without adding `libpkgreconcile*` to production dependency
closure. Do not create a pass-through controller wrapper merely to make the test
look more integrated.

## Documentation authority

Current-facing documentation is normative for the current controller boundary:

- `README.md` describes the current product and command surface;
- the newest release section at the top of `DESIGN.md` describes current
  authority composition;
- older design sections are historical and must be labelled explicitly when a
  later release replaced their mechanism;
- `TESTING.md` states the current qualification contract;
- installed manual pages describe only current observable behavior; and
- `CHANGELOG.md` may preserve superseded behavior as release history.

A historical mechanism must not remain in a current testing requirement,
maintenance rule, manual contract, or unqualified design statement. When an
authority moves, update the owner documentation and add a negative documentation
contract for the retired wording in the same change.

## Dependency direction

The executable directly depends on the exact libraries whose public values it
uses. The internal `pkgctl-core` library is not installed and does not publish a
second package-management API. A direct provider SONAME transition must be
reflected in both Meson and direct pkg-config qualification rather than relying
on a transitive consumer to reject the old generation. Shared release
qualification must prove the expected direct ELF dependency for providers whose
effectful API the controller calls, including `libpkgfetch`.

The construction layer may depend on source, fetch, build, build-exec, image,
and backend-neutral execution authorities, but must not schedule dependency
graphs or construct a Linux backend.

The preparation layer may depend on the published state-plan, source-plan,
build-plan, planner, image, and application values. Incoming preparation must
consume the exact `libpkgbuild-image` authority retained by construction and use
the pure `libpkgbuild-plan` projection. It must not reopen artifact bytes,
select an inspection backend, re-prove payload/image equality, observe target
paths itself, normalize package policy, discover runtime closure, execute
lifecycle programs, mutate the target, or publish state.

The check layer may depend on transaction progression, construction evidence,
`libpkgcheck`, and `libpkgcheck-exec`. Pure request admission must remain usable
before host paths exist. Concrete resources, interpreter identity, credentials,
and limits belong only to the admitted check session. The controller must not
reimplement logical-input/resource matching, resource-layout construction,
process-status classification, or backend execution.

The pure dispatch layer may depend on transaction progression and exact admitted
construction, check, and effect sessions. It owns deterministic reservation,
bounded in-flight capacity, caller attempt nonces, and immutable ownership
records. It must not execute a driver, create a backend, allocate resource paths,
construct new transaction edges, or infer success from reservation state.
Check-scoped package inputs require `libpkgtransaction >= 3.0.0` so exact input
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

The native operation-authority layer may translate one exact transaction,
progress epoch, durable record, and operation dispatch into an admitted effect
session. It may consume one replayable per-dispatch target/planning
specification, explicit lifecycle execution order, the exact predecessor
construction for incoming operations, fixed lifecycle coordinates and
credentials, the exact latest effect record, and caller-owned
restart bodies. Replayable archives may be selected only by an explicit
incoming-authority map and opened under the retained archive digest. This layer
must not discover paths, observe or mutate the target, execute effects, append
journals, read canonical state, choose backends or credentials, retry, schedule,
or expose a command.

The native POSIX transaction-run runtime layer owns mechanical lifetime and
wiring authority for one sealed transaction. It may retain caller-selected
run-store, construction/check evidence-store, effect-store, and target-lock
directories; construct the concrete native session, operation, archive,
progress-rehydration, recovery, driver, and dispatch-nonce chain; and delegate
one bounded launch or exact-journal drive. Each launch must receive an explicit
caller run nonce because that value selects caller intent and must not be
inferred from transaction semantics.

The runtime must continue to borrow semantic owner inputs: retained installed
package trees, live per-dispatch operation specifications, subordinate restart
bodies, selected backends, and canonical state. It must not freeze target facts
for later operations, discover paths or journals, initialize stores, generate
or persist run intent, invent semantic evidence, select credentials or
backends, retry, wait, schedule, clean up, or add a frontend command.
Descriptor-anchored authority must remain valid if the original pathname is
renamed or replaced, and all four runtime namespaces must be disjoint both by
selected path and retained filesystem identity.

Fresh and recovered construction/check work must derive from one shared
deterministic session source. Do not introduce a second recovery-only session,
path, credential, root-view, package-input, or workspace provider. The exact
execution request must be reproduced through the pure build/check adapter
projection; recovery must never call effectful `prepare()` merely to obtain
request authority. Construction may reacquire genuine source material through
`libpkgfetch`; operation restart remains delegated to the effect journal.

The construction/check evidence layer may serialize only one exact typed
dispatch-evidence record and the existing canonical subordinate result encoding.
It must bind the run journal, transaction, dispatch, node, attempt, controller
request/result, and subordinate context identities before publication. The
content object must become durable before the typed index, and terminal run
retirement must follow evidence publication. The store may validate encoding and
index integrity but must not decode a build/check result without the complete
original request, execution-request, backend-profile, source-materialization,
and resource authorities. It must not discover those authorities, scan indexes,
or promote an identity into semantic evidence.

The evidence-backed recovery layer may select only the exact typed index named
by the committed journal, dispatch, and attempt. It must obtain complete context
bodies from a caller-owned source, prove every body against the identities in the
durable record, invoke the existing subordinate decoder, and reproduce the
canonical controller-result identity before returning recovery authority. It
must treat absent evidence as unresolved started ownership, not as a releasable
reservation. It must not discover paths, reconstruct a request from identities,
substitute a current backend profile, accept a semantically similar session, or
parse subordinate bytes through a second codec. Operation recovery remains an
effect-journal responsibility and must not be routed through construction/check
objects.

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
stopped or quiescent runs, or after any stopping outcome. A durable operation
step that retains a result while leaving its dispatch started is already an
external-resolution stopping outcome; stop on that same step rather than taking
a non-durable rediscovery iteration. Continued steps must remain in one journal
and strictly advance durable head sequence. The layer must
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
   recovery, immutable construction/check evidence publication before
   terminal retirement, corrupt/missing/conflicting evidence refusal,
   descriptor-anchored evidence reopening, preparation projection and typed
   refusal, effect sequencing,
   intent-before-effect persistence, exact restart checkpoints, outer-lease
   reacquisition, publication reconciliation, publication provenance, CLI
   read-only behavior, and missing-state refusal;
6. update release metadata and manuals together;
7. compare independently replayed trees and stable patch IDs;
8. tag signed releases only from a clean tree.
