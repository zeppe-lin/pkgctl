PKGCTL_ORCHESTRATION(7)

# NAME

pkgctl_orchestration - native pkgctl authority composition

# DESCRIPTION

*pkgctl* is a controller, not a package-model authority. It decides which exact
component to call next, retains the resulting handoff identities, and reports
the resulting control state.

# BOUNDED NATIVE TRANSACTION COMMAND

Version 0.35.0 exposes *pkgctl run*, one narrow command over the existing native
runtime. *--start* composes from the explicit transaction request, then
immutably retains the one current private command-evidence format: the complete
start-only transaction inputs, exact admitted interpreter identity, exact admitted
construction/check/lifecycle backend capability profiles, plus the owner-encoded
catalog/state universe, before admitting one caller-issued run
nonce. *--resume* requires the retained command evidence and exact admitted journal and
recomposes the same transaction identity without a second semantic request.
Catalog acquisition, target-binding, architecture, goal, resolution-policy, and
convergence options are start-only. Resume accepts the current canonical-store
pathname as live physical authority while taking its target binding from retained
state evidence. It does not reacquire collections or substitute current state for
the historical epoch. Command evidence retains its admitted backend profiles through
the libpkgexec owner codec. Construction/check attempt evidence separately retains the
exact libpkgexec-owned profile body required to decode its historical result. Lifecycle
result evidence carries that exact profile body inside the libpkgapply-exec owner
encoding. Recovery therefore does not query a current backend or inject a command-level
profile as historical result authority. Current
interpreter observation, live capability, and supervisor-credential preflight are
required only for execution scopes that can still run. Bytes outside the current private command-evidence format
have no compatibility path.

The command-private effect-body store is both durable sink and restart source.
Lifecycle results, application receipts, publication requests, and publication
receipts are retained through their owner codecs before an effect journal may
name them. Construction-run evidence also retains the exact admitted
controller-owned construction session plus owner-encoded fetch/build evidence;
check-run evidence retains its exact admitted controller session plus owner-
encoded check evidence. Restart validates and decodes those sessions under the
retained transaction/progress/node authority without reconsulting the current
session locator, construction/check configuration, or installed-package resource
source. Application-intent restart uses the direct
*libpkgapply-posix* active-request index; no directory scan or controller-owned
application-journal format is introduced.

Target observations are acquired only for the current operation dispatch. Before
a started operation can be named durably, the command retains its complete admitted
operation session and, for incoming operations, the exact construction-artifact
path bound to that incoming authority. Restart decodes those retained authorities;
it does not replay the live operation specification, target observation, or
predecessor construction lookup.
Runtime dependency authority is the exact transitive resolver *run*-scope
closure rooted at that package, not transaction graph storage order or SCC
cohort representation. Application mutation/observation identities remain
separate from lifecycle process-execution capability identity.

Construction/check execution and lifecycle execution also remain separate
command authority domains. Their existing root views and explicit numeric
credential sets are supplied independently. The current native Linux backend
admits only the invoking supervisor's credentials; a relevant mismatch is
control-plane refusal before run admission. No fakeroot, logical ownership
virtualization, or implicit credential transition is provided.

All runtime namespaces, construction/check root, lifecycle root, and target root
must already exist. One
invocation performs at most the positive *--max-steps* drive policy. It starts
no daemon, discovers no journal, performs no unbounded loop, timer retry,
implicit replanning, rollback, repair, cleanup, compaction, or garbage
collection. This is the final functional package-management boundary; the next
work is coordinated qualification and publication.

# NATIVE TARGET AND RUNTIME COMPOSITION

Version 0.34.0 provides *native_posix_transaction_run_runtime*, the stable
native composition root for one sealed transaction. It owns the POSIX
transaction-run, construction/check evidence, and effect stores; the native
construction/check locator; native operation execution and restart authority;
explicit archive mapping; exact progress rehydration; restart composition;
construction, check, and effect drivers; and canonical dispatch nonce
projection.

The caller supplies four existing disjoint POSIX namespaces, fixed
construction/check and lifecycle coordinates, retained installed-package tree
authority, live per-dispatch operation specifications, subordinate effect
restart bodies, selected physical backends, and canonical state. Path-based
opening requires absolute normalized existing directories and uses final-component no-follow
directory semantics. Descriptor-based opening rejects aliases by filesystem
identity. No store or directory is initialized, discovered, scanned, or
created.

Construction/check and lifecycle execution retain separate typed root-view
authorities. If they deliberately name the same host path, their identities
must agree. Build/check writable roots must not overlap lifecycle execution,
target, or lifecycle session authority. Fresh construction/check sessions depend on exact semantic progress rather than
caller run intent. Once an attempt starts, its admitted controller session is
retained with durable evidence; progress/restart rehydration decodes that exact
session and no longer calls the fresh session locator. Terminal operations
are reopened through the same native operation authority and canonical
checkpoint validation.

Every launch requires an explicit durable run-intent nonce and a positive bound.
The runtime does not itself select backends or credentials, freeze target
observations, schedule, retry, wait, clean up, compact, or collect history.
Version 0.35.0 supplies those explicit command authorities without moving them
into this composition root.

# NATIVE OPERATION AUTHORITY

Version 0.33.0 implements one native operation execution and recovery source
for an exact sealed transaction. A replayable per-dispatch source supplies the
current action's target context, application execution control, planner
observations and policy, runtime closure, explicit lifecycle execution order,
and installation reason where required. Fixed configuration supplies lifecycle
coordinates and credentials.

Fresh authority consumes the exact current progress and predecessor
construction and uses the existing preparation boundary. The effect boundary
validates that the supplied lifecycle order contains exactly the nodes attached
to the action by transaction phase edges; deterministic graph storage does not
become execution precedence. The source then admits each deterministic lifecycle
scratch session as one direct child of the caller-provisioned session root. The
child identity is domain-separated by stable run journal, dispatch, lifecycle
phase, and phase index; the source creates no directories, while
*libpkgapply-exec* creates and protects the single-use leaf. It issues one
domain-separated mechanical effect-attempt nonce for the exact durable head.
Restart loads and decodes the retained admitted session, loads the exact active
effect record, obtains subordinate semantic bodies from their owner, and invokes
the canonical effect checkpoint validator. Live operation specification and target
observation are fresh-dispatch authority only.

An explicit replayable archive source maps one admitted incoming authority to
one absolute retained path and opens it through a caller-selected *libpkgimage*
backend with the authority's exact complete-archive digest. It performs no
archive discovery or filename inference.

This boundary performs no filesystem mutation, target observation, lifecycle or
application execution, effect-journal append, canonical-state read,
publication, backend selection, credential discovery, retry, scheduling, or
command actuation. The command surface remains read-only.

# EXACT PROGRESS REHYDRATION

Version 0.32.0 implements one store-backed
*transaction_progress_rehydration_source*. It begins from the sealed
transaction and replays only completed durable dispatches when their exact graph
unit becomes ready. Construction and check records are selected by journal,
dispatch, and attempt identity and decoded by their owning codecs under
caller-supplied complete bodies. Completed operations require one exact terminal
effect record and validated checkpoint.

Successful operation state is derived by the pure *libpkgstate* publication
projection. Rehydration does not read canonical state, continue execution,
append journals, publish, retry, repair, scan paths, or mutate the target.
Reserved, started, released, and nonterminal histories remain unresolved control
ownership. The final semantic progress and state epoch must reproduce the durable
run record exactly.

# READ-ONLY PIPELINE

Version 0.21.0 retains the read-only command pipeline introduced in 0.2.0:

. explicit collection specifications through *libpkgcatalog-acquire*;
. one sealed *libpkgcatalog* snapshot;
. one existing *libpkgstate* snapshot for an exact target binding;
. one *libpkgresolve* request and result;
. one *libpkgtransaction* request and program.

The canonical state store is opened with *open_existing* for these commands.
The command frontend does not initialize or publish state. Version 0.19.0 also
opens one explicitly named existing POSIX transaction-run store for
*inspect-run*, and version 0.21.0 opens one explicitly named existing POSIX
effect-attempt store for *inspect-effect*. Neither command creates its store or
writer lock.

# EXPLICIT RUN INTENT AND CANONICAL DISPATCH NONCES

Version 0.27.0 derives every logical construction and check input from the
exact resolver result retained by the transaction. Callers supply only
call-scoped resource identities and host paths for those exact inputs. Operation
preparation consumes the construction result's retained build/image admission
and the standalone planner projection; it does not reopen artifact bytes.

Version 0.26.0 requires one explicit *transaction_run_nonce* on each runtime
*launch*. The run nonce distinguishes caller intent between otherwise identical
initial runs. Reusing it selects the same exact durable journal; changing it
selects another history. The runtime does not generate, persist, discover, or
infer this intent value.

Fresh dispatch nonce authority is mechanical. The runtime owns one stateless
*canonical_transaction_dispatch_nonce_source*. Before derivation it proves that
the supplied transaction run is the exact semantic reopening of the committed
journal record. It then hashes the journal, record, and run identities under a
dedicated controller domain. An exact retry against one head derives the same
nonce; every legal committed successor derives another.

This mechanism uses no random generator, seed file, nonce database, hidden
cache, journal scan, or process-lifetime state. It changes no command syntax and
adds no mutating command.

# CALLER-CONFIGURED POSIX TRANSACTION-RUN RUNTIME

Version 0.25.0 provides *posix_transaction_run_runtime*, a mechanical
composition root for the existing durable transaction controller. The caller
supplies already-open directories for transaction-run journals,
construction/check evidence, effect-attempt journals, and target-mutation locks.
The runtime retains those four directory authorities and owns one POSIX run
store, one POSIX construction/check evidence store, one POSIX effect store, native
construction and check drivers, and one caller-configured POSIX per-dispatch
effect source.

The runtime borrows semantic progression rehydration, exact dispatch execution and recovery-context
sources, archive lookup, construction and check execution
backends, application and lifecycle backends, and the canonical state store.
The caller must keep every borrowed authority alive. Run intent is supplied
explicitly to *launch*, while dispatch nonce derivation is owned mechanically by
the runtime. No durable identity is used to discover or reconstruct semantic or
physical package authority.

*launch* accepts exact initial progression, a dispatch policy, one explicit run nonce, and a positive
drive bound. It delegates restart-safe admission and bounded driving to the
existing controller. *drive* accepts one exact journal identity and a positive
bound; it performs no run enumeration or latest-run selection. Both operations
retain all existing write-ahead reservation, execution, effect, publication,
and recovery barriers.

The runtime performs no store initialization, path or journal discovery, nonce
policy, semantic evidence construction, backend or archive selection,
credential selection, waiting, retry loop, scheduling, worker creation, cleanup,
repair, or compaction. Version 0.25.0 changes no command syntax and adds no
mutating command.

# CALLER-CONFIGURED POSIX EFFECT RUNTIME

Version 0.24.0 provides *posix_transaction_effect_driver_source*, a concrete
implementation of the per-dispatch source. The caller supplies an already-open
lock-directory descriptor, one application backend, one lifecycle execution
backend, one canonical state store, and one replayable-archive source. The
source duplicates the directory descriptor and borrows the remaining
mechanisms; it does not derive them from journal identities or discover them
from the host.

For an incoming package, *acquire_transaction_effect_archive* asks the caller's
source for one archive and validates its package-image and inspection-receipt
identities against the exact admitted incoming authority. A removal requests no
archive. Archive admission occurs before the target lease is acquired.

Fresh execution acquires a nonblocking POSIX outer target lease and derives the
installed-state projection with the lease-bound *libpkgstate-apply* adapter. The
continuation driver and resulting-state observer retain one shared lease
acquisition. Recovery receives the exact classifier-selected shape: the shared
pair for continuation, a target-scoped observer for terminal success, a
publication driver for publication reconciliation, or no authority for terminal
failure and external resolution. Nonblocking lock contention is not an effect
outcome. The POSIX source translates only *lock_busy* into the controller's
*mutation-authority-unavailable* disposition. Fresh contention releases the
unstarted reservation without admitting an effect attempt; recovery contention
leaves existing run/effect ownership unchanged. No waiting or retry occurs inside
the bounded drive.

The source performs no path discovery, state-store initialization, backend or
credential construction, archive enumeration, waiting, retry, scheduling,
journal I/O, cleanup, or policy selection. Version 0.24.0 changes no command
syntax and adds no mutating command.

# SPLIT EFFECT CONTINUATION AND STATE AUTHORITY

Version 0.23.0 separates physical continuation from canonical-state observation
and publication reconciliation. A fresh operation receives one continuation
driver and one resulting-state observer. Both are validated against the exact
operation target and must name the same live mutation-lease acquisition before
the effect attempt is admitted.

Restart classification selects the exact authority subset. Lifecycle or
application continuation receives the old lease-bound state projection plus a
separate resulting-state observer. A successful terminal effect receives only
the observer. Publication-intent or indeterminate-publication recovery receives
only a publication driver, which can read canonical state and retry the exact
durable request under a target-scoped lease. Terminal failure and
external-resolution states receive no physical authority.

This split prevents recovery from manufacturing the obsolete state projection
after publication may have advanced the canonical store. Sources returning
missing or surplus authority, a foreign target, an expired lease, or different
lease acquisitions are refused. Authorities remain call-scoped and are neither
serialized nor retained.

Version 0.23.0 adds no concrete native source, path or credential discovery,
lease acquisition, archive selection, backend assembly, scheduler, worker,
retry policy, cleanup, or mutating command.

# PER-DISPATCH EFFECT-DRIVER AUTHORITY

Version 0.22.0 replaces the run-wide operation driver with a caller-owned
*transaction_effect_driver_source*. After an operation reservation is committed
and the exact semantic execution handoff is admitted, the source returns one
call-scoped driver for that dispatch. Before effect admission, *pkgctl* validates
its live target mutation lease and proves that its state projection matches the
operation session's exact expected snapshot and ownership inventory.

Restart first classifies the retained effect checkpoint. A recovery driver is
requested only when continuation can touch the target or a successful terminal
operation requires a resulting-state read. Terminal failure and
external-resolution dispositions invoke no driver source. Source refusal or an
invalid driver leaves the operation reserved and creates no effect-attempt
journal.

The driver source does not discover policy inside *pkgctl*. It remains
responsible for caller-selected leases, state projections, archives, backends,
credentials, and paths. Drivers are neither serialized nor retained across
advancement calls. This version adds no native runtime assembly and no mutating
command.

# EXACT EFFECT-INSPECTION COMMAND

Version 0.21.0 exposes the durable effect-attempt sensor as *pkgctl inspect-effect*. The caller supplies an existing POSIX effect-store path and one
exact lowercase SHA-256 attempt identity. The frontend opens only that store,
loads only that attempt, delegates to *inspect_effect_attempt*, and renders the
existing deterministic report.

The POSIX read path acquires an existing lock through a shared read-only
descriptor. If a caller-created store has no lock, the reader creates nothing;
if a writer establishes the lock concurrently, the observation is repeated
under that lock. Inspection therefore does not require write access and cannot
initialize the store.

The command does not enumerate attempts, select a latest attempt, traverse a
run journal, rehydrate lifecycle, application, transaction, publication, or
state semantics, construct restart authority, invoke a driver, append,
reconcile, repair, schedule, or mutate. Invalid identity syntax is a usage
error. Missing stores, missing heads, corrupt stores, and storage-authority
violations retain typed effect-journal diagnostics.

# DURABLE EFFECT-ATTEMPT INSPECTION

Version 0.20.0 can inspect one exact caller-selected durable effect attempt
without changing it. The controller loads the committed head, validates that
the store returned the same attempt authority, and pairs the exact retained
*effect_attempt_record* with the existing pure *effect_restart_assessment*.

The report prints the controller-owned attempt, record, session, nonce,
predecessor, sequence, stage, lifecycle-result, application, transaction,
publication, terminal, and reconciled-state identities. Optional facts are
omitted until retained. An identity remains an identity: inspection does not
reconstruct lifecycle results, application receipts or journals, semantic
transaction evidence, publication requests or receipts, or state snapshots.

An existing POSIX effect-store lock is opened read-only and acquired shared. If
the lock is absent, the reader creates nothing and rechecks after its unlocked
observation in case a writer established the lock concurrently. Append remains
the only read-write, lock-creating, exclusive path.

The sensor performs no attempt enumeration, latest-attempt discovery, run-journal
traversal, semantic restart rehydration, driver invocation, append,
reconciliation, repair, scheduling, or mutation. Version 0.21.0 exposes only
this exact read-only sensor through the command frontend.

# EXACT RUN-INSPECTION COMMAND

Version 0.19.0 exposes the durable inspection sensor as *pkgctl inspect-run*.
The caller supplies an existing POSIX run-store path and one exact lowercase
SHA-256 journal identity. The frontend opens only that store, loads only that
journal, delegates to *inspect_transaction_run*, and renders the existing
deterministic report.

The POSIX read path acquires an existing lock through a shared read-only
descriptor. If an empty caller-created directory has no lock, the reader creates
nothing; if a writer establishes the lock concurrently, the read is repeated
under that lock. Inspection therefore does not require write access and cannot
initialize the store.

The command does not enumerate journals, select a latest run, rehydrate semantic
progression, inspect effect journals, append, reserve, execute, reconcile, repair,
schedule, or mutate. Invalid identity syntax is a usage error. Missing stores,
missing heads, corrupt stores, and storage-authority violations retain typed
run-journal diagnostics.

# DURABLE TRANSACTION-RUN INSPECTION

Version 0.18.0 can inspect one exact caller-selected transaction-run journal
without changing it. The controller loads the committed head, validates that the
store returned the same journal authority, and classifies the record as
*completed*, *stopped-after-failure*, *active*, or *quiescent-incomplete*.

Active dispatches retain their exact restart disposition: release an unstarted
reservation, recover construction, recover a check, or inspect the exact effect
journal. The report states whether external evidence is required and prints the
controller-owned journal, record, sequence, transaction, run, progression, state,
policy, dispatch, attempt, effect-attempt, observation, and terminal identities.

Inspection does not rehydrate package semantics or promote an identity into
evidence. It performs no append, reservation, execution, subordinate journal
inspection, journal discovery, scheduling, or cleanup. Version 0.19.0 exposes
this sensor through a read-only command, but still exposes no effect-implying
command.

# CANDIDATE CONSTRUCTION

Version 0.5.0 can construct one exact catalog-backed transaction *build* node.
The caller supplies the exact package-input facts and trees, source/store and
build coordinates, closed build policy, interpreter, credentials, and an
injected backend-neutral construction driver.

The controller validates the node and resources before effects, materializes
the sealed source snapshot through *libpkgfetch*, seals the exact *libpkgbuild*
request from observed digests and resolver architectures, and executes one
*libpkgbuild-exec* session. Completion requires successful build evidence and
independent archive inspection through *libpkgimage*.

Host paths, cache hits, redirect observations, and timing remain outside
construction identity. The boundary does not recursively schedule dependencies,
execute check nodes, publish installed state, or construct a Linux backend.
Interrupted construction is not inferred successful or automatically replayed.

# TRANSACTION PROGRESSION

Version 0.8.0 retains immutable controller knowledge for one transaction
program. Progression starts at the exact installed snapshot used by resolution
and records accepted terminal construction and effect evidence.

Every *build* node is one construction unit and every *check* node is one check
unit. A target *install*, *upgrade*, or *remove* node absorbs the lifecycle nodes
connected by the exact pre-action and post-action phase edges. The operation unit
becomes ready only when all predecessors outside that unit are satisfied.
Internal lifecycle edges therefore do not deadlock selection of the operation
that must execute them.

Progression derives *pending*, *ready*, *satisfied*, *failed*, and *blocked*
status for every node and exposes all ready units. It does not choose among
ready peers. A successful operation satisfies its complete member set. A failed
operation retains exact status for lifecycle nodes that executed and blocks
members that were never executed.

A successful effect advances the current state epoch only when a matching
*libpkgstate* publication receipt or authoritative restart reconciliation proves
the supplied resulting snapshot. Definitive failure does not advance state.
Lost-lease and indeterminate-publication outcomes require further resolution and
are not terminal progression evidence.

Version 0.8.0 admits a pure check request only for one exact ready check node
whose unique build predecessor already has successful retained construction
evidence. Concrete source, built-package, check-input, root-view, temporary,
interpreter, credential, and resource-limit coordinates are admitted separately
through *libpkgcheck-exec*.

Terminal check evidence must bind the exact check and execution requests. Passed
evidence satisfies the check node; failed evidence fails it and blocks dependent
units. Completion may follow unrelated progression advancement only while the
same node remains ready and the same construction evidence remains retained.

# TRANSACTION DISPATCH

Version 0.9.0 adds an immutable *transaction_run* ledger for ready units that
have been selected but have not yet produced terminal evidence. Progression
continues to expose all ready units; dispatch policy may reserve the first unit
in canonical order that fits explicit construction and check capacities. Target
operations use one non-configurable mutation lane.

Each reservation binds a nonzero caller-issued 32-byte nonce, the exact ready
unit, the current state epoch, and terminal evidence for every external
non-retain predecessor. Nonces and records remain in the run after completion or
release, preventing silent attempt reuse.

A dispatch is first *reserved*. It becomes *started* only when an exact admitted
construction, check, or effect session is bound. Only a reserved dispatch can be
*released-unstarted*. Started work remains owned until authoritative terminal
evidence arrives. Construction and check results must belong to the exact
started session. Operation results that report lost lease or indeterminate
publication remain ordered observations on an active dispatch; they cannot
carry a resulting state snapshot.

Before construction starts, every package input must match one exact
*libpkgtransaction* requirement edge and exact selected or retained package
truth. Build and artifact identities must reproduce successful predecessor
construction evidence. Retained installed inputs must still be the same package
in the current canonical state. *libpkgtransaction* Version 3.0.0 or later orders
both build- and check-scoped package inputs before the construction that seals
them.

The only containment policy in this release stops new reservations and starting
merely reserved work after terminal failure. Already-started independent work
may still submit exact terminal evidence. The pure dispatch model creates no
threads, invokes no driver, builds no backend,
allocates no resource path, persists no run, and exposes no
effect-implying command.

# DURABLE RUN ADMISSION

Version 0.16.0 can commit sequence zero of one transaction-run history. A
library caller supplies exact initial progression, explicit dispatch policy, an
injected replay-safe run-nonce source, and one run store. The controller
constructs the immutable initial run before requesting the nonce, appends the
sequence-zero record, validates the complete authority returned by storage, and
reopens the run from that committed record.

Exact retries for the same initial run must receive the same nonce and converge
on the same record. Nonce-source refusal performs no store write. Store failure
grants no admitted controller authority. The admission contains no predecessor
and no dispatch ownership. It does not reserve or execute work, access an effect
journal, discover resources, schedule, loop, choose retry timing, roll back,
clean up, compact history, collect garbage, or expose an effect-implying
command.

# RESTART-SAFE TRANSACTION LAUNCH

Version 0.17.0 can converge on one exact transaction-run journal and drive it
under an explicit positive bound. The caller supplies initial progression,
dispatch policy, replay-safe run and dispatch nonce sources, semantic authority
sources, drivers, and explicit stores.

The controller constructs the immutable initial run and requests its run nonce
before storage. That pair derives one exact journal identity. If the store has no
committed head for the journal, sequence zero is appended and validated before
the drive begins. If a head already exists, its transaction, nonce, and
dispatch policy must match the expected admission universe; no sequence-zero
append is attempted.

This makes launch retryable after interruption on either side of admission. A
failed admission append performs no drive action. A later failure leaves the
committed journal for exact restart. Retrying after successor records exist
resumes the current head; retrying after completion returns completed through
quiescence without invoking a driver again.

The call performs no journal scan, unbounded execution, worker creation,
concurrency, adaptive scheduling, retry timing, resource or evidence discovery,
process adoption, rollback, cleanup, compaction, garbage collection, or
effect-implying command.

# DURABLE CONSTRUCTION AND CHECK EVIDENCE

Version 0.28.0 commits construction and check evidence after driver completion
and before terminal run retirement. The exact order is:

. commit the started transaction-run successor;
. execute the already admitted construction or check session;
. admit one typed dispatch-evidence record;
. publish the immutable canonical subordinate result object;
. publish the immutable journal/dispatch/attempt index;
. commit the terminal transaction-run successor.

A failure before evidence publication leaves the started dispatch durable and
publishes no terminal successor. A failure of the terminal run commit leaves the
same started ownership together with exact loadable evidence. An exact retry may
reuse identical content and index objects; conflicting publication for one
journal, dispatch, and attempt fails closed.

The POSIX store is anchored to an explicit directory descriptor. It publishes
content before index and synchronizes every commit point. Read-time validation
rejects malformed encoding, wrong identities, corrupt indexes, absent indexed
objects, altered content, non-regular objects, and conflicting publication.
Renaming or replacing the original pathname does not change the retained
directory authority.

The evidence record contains the existing canonical *libpkgbuild-exec* or
*libpkgcheck-exec* result encoding and the exact context identities required to
validate later rehydration. The stored identity is never promoted into semantic
evidence. The store does not call the subordinate semantic decoders because
those require complete original request, execution-request, backend-profile,
and, for construction, source-materialization authority. Supplying those bodies
remains the responsibility of a later caller-owned recovery provider.

This boundary performs no record discovery, semantic reconstruction, resource
realization, backend selection, process adoption, retry policy, scheduling,
cleanup, compaction, garbage collection, or command action.

# NATIVE CONSTRUCTION AND CHECK SESSION LOCATION

Version 0.31.0 implements one private native
*transaction_dispatch_session_source*. It consumes the already sealed
transaction and exact current progress together with one durable run journal,
one dispatch, explicit configured roots and execution policy, and a
caller-owned retained-installed-package tree source.

For a catalog subject, the retained source document is accepted only when it is
an absolute native *recipe.yml* coordinate directly beneath the exact collection
root retained by acquisition authority. For catalog-selected inputs, exactly one
successful predecessor construction must supply the package-output resource and
path. For installed-selected inputs, the caller's source must return a tree for
the exact installed package identity. No package authority is inferred from a
pathname or digest.

Construction session, package-output, artifact, and check-temporary coordinates
are derived beneath disjoint configured roots from the stable journal and
dispatch identities. A reserved record and its started successor therefore
reproduce the same session. Check sessions reuse source, output, and input
resources from the successful predecessor construction. The build adapter's
pure prepared-path projection owns the staged source coordinate.

Location performs no filesystem observation or mutation, source
materialization, backend construction, execution, evidence decoding, journal
I/O, or progress advancement. It is not a target-profile parser and does not
open stores or choose backends. Version 0.31.0 changes no command syntax and
adds no mutating command. Exact transaction-progress rehydration is the next
unimplemented controller provider.

# SHARED CONSTRUCTION AND CHECK SESSION AUTHORITY

Version 0.30.0 makes one deterministic session source authoritative for both
fresh construction/check execution and restart recovery. Given the same durable
record, reopened run, and dispatch, the source must reproduce the same admitted
session. The runtime composes that source with separate operation execution and
operation recovery authorities; operation restart remains owned by the effect
journal.

The build and check execution adapters expose pure request projections. These
projections bind the exact semantic request, logical resources, root view,
program, environment, credentials, limits, and cancellation policy without
touching host paths. Effectful preparation uses the same request and remains the
only boundary which stages source bytes or binds concrete materializations.

Construction restart reacquires the exact source materialization through
*libpkgfetch*, admits the build-exec session, reproduces the execution request,
and reads the selected backend capability profile. Check restart reproduces its
execution request from the admitted check session and selected check backend.
The retained evidence record then validates and decodes those bodies exactly as
in version 0.29.0.

This boundary does not implement session discovery or concrete path policy. The
caller still owns package-input resources, predecessor artifacts, retained
installed-package resources, root views, credentials, workspaces, and artifact
destinations.

# EVIDENCE-BACKED CONSTRUCTION AND CHECK RECOVERY

Version 0.29.0 composes one exact durable construction/check record with the
semantic bodies required by the existing subordinate decoder. The store-backed
recovery source first selects by journal, dispatch, and attempt session. It then
asks a caller-owned context source for the exact admitted controller session,
execution request, backend capability profile, and, for construction, source
materialization.

Every supplied body must reproduce the transaction, graph node, controller
request, build/check request, retained construction, materialization, execution
request, and backend identities recorded durably. Only then may the source call
the canonical *libpkgbuild-exec* or *libpkgcheck-exec* decoder. The decoded
execution and result identities are checked again, and the private controller
result is accepted only when the original canonical result identity is
reproduced.

No durable identity is promoted into a semantic body. Missing evidence means a
started dispatch has no recoverable construction/check execution record; it does
not mean that execution never began and does not release the reservation.
Foreign or contradictory context and subordinate decoder refusal fail before
run reconciliation. Operation recovery remains delegated to the exact effect
journal checkpoint and never consults this evidence store.

The POSIX transaction runtime owns the store-backed recovery source and borrows
only the context provider. This boundary does not discover sessions, resources,
paths, source objects, execution requests, backends, or processes; choose a
retry; or add a command action.

# SINGLE-DISPATCH EXECUTION

Version 0.11.0 can realize one exact caller-selected dispatch. It does not select
or reserve work. The caller supplies the current committed run record, its exact
rehydrated run, the retained dispatch, one admitted session, an injected driver,
and explicit stores.

Construction and check start by appending the exact *started* run successor.
Only after that append returns the same committed authority may the driver run.
Returned construction/check evidence is admitted and published through the
exact evidence store before the ordinary completion function may append the
terminal successor. A failed start append invokes no driver. Driver escape,
evidence-store failure, or a failed final append leaves the started dispatch
selected for exact recovery; after a final-append failure, the evidence remains
loadable.

Operation start first appends effect-attempt admission, then appends the started
run retaining that attempt identity. Both commits precede every physical effect.
Execution continues through the durable effect journal. Successful publication
is paired with a fresh installed-state read before run completion. Lost lease or
indeterminate publication remains an observation on active ownership. A terminal
effect journal whose run completion was not committed is recovered by inspecting
the retained effect-attempt identity.

This layer creates no scheduler, loop, thread, backend, resource-discovery rule,
retry rule, rollback, automatic release, journal discovery, or effectful command.

# BOUNDED SERIAL TRANSACTION DRIVE

Version 0.15.0 can drive one durable transaction journal serially for an
explicit positive maximum number of steps. Every step delegates to the one-step
advancement boundary and therefore reloads the exact committed run head before
semantic authority is requested.

Retained ownership has precedence. A reserved or started dispatch is reconciled
without requesting a fresh dispatch nonce. If the reopened run is stopped or
has no canonical ready work, the call returns without nonce issuance. Only a
head that can reserve fresh work is passed to an injected replay-safe
*transaction_dispatch_nonce_source*. Exact retries against the same record must
return the same nonce. A committed successor record may define a new nonce.

This head-derived contract prevents caller preallocation from consuming attempt
identity before durable ownership exists. Failure after nonce issuance but
before reservation commitment leaves the same head for exact retry. Once
reservation commits, restart sees retained ownership and reconciles it before
any later nonce is requested.

The drive stops on transaction completion, terminal failure containment,
operation external-resolution authority, incomplete quiescence, or exhaustion
of the explicit bound. The returned ordered outcomes must remain in one journal,
and every continued outcome must expose a strictly newer committed record. No
outcome may follow a completed, stopped, externally blocked, or quiescent one.

The drive is bounded serial composition, not scheduling policy. It creates no
worker, concurrency, adaptive priority, unbounded loop, retry timing, backoff,
resource or evidence discovery, process adoption, rollback, cleanup,
compaction, garbage collection, or command action.

# ONE-STEP TRANSACTION ADVANCEMENT

Version 0.14.0 composes the durable run, exact authority, execution, and restart
boundaries into one bounded call. The caller supplies an exact journal identity,
a dispatch nonce for possible fresh work, caller-owned authority sources,
explicit drivers, and explicit stores.

The call loads the committed run head from the run store and rehydrates its exact
semantic progression. If the run retains active ownership, the first active
dispatch in durable ledger order is reconciled and the call returns. Reserved
work is released. Started construction and check work require exact recovered
evidence. Started operation work inspects the exact retained effect attempt. An
external-resolution disposition returns the unchanged committed run, invokes no
driver, and appends neither journal. Recovery therefore has precedence over every
new reservation.

Only a quiescent reopened run may reserve the first canonical ready dispatch.
The selected driver and any required effect store are validated before the
reservation append. The reservation is committed before fresh execution
authority is requested. The admitted authority then passes through the existing
single-dispatch write-ahead barriers: durable start before driver invocation and
durable terminal evidence after the ordinary completion transition.

A failed authority source leaves a durable reservation. Driver escape or failed
terminal commitment leaves durable started ownership. The returned result is
validated against the storage-derived record and can describe only one exact
quiescent, released, reconciled, externally blocked, or freshly executed
outcome.

One call either reconciles retained ownership or executes one newly reserved
dispatch and then returns. It creates no loop, scheduler, worker, concurrency,
retry or backoff policy, resource or evidence discovery, process adoption,
rollback, cleanup policy, compaction, garbage collection, or command action.

# EXACT RUN-AUTHORITY REHYDRATION

Version 0.13.0 accepts caller-owned sources for the exact authorities needed to
resume or execute one durable dispatch. It does not search for those authorities.

*rehydrate_transaction_run()* obtains one complete *transaction_progress* from
an injected source and reopens the durable record through the ordinary restart
checkpoint validator. A stored identity is never promoted into semantic
evidence.

For fresh execution, the source returns one admitted construction session,
check session, or operation session plus caller-issued effect nonce. The
controller validates that authority against the exact reserved dispatch using
the existing pure start transition and seals a call-scoped handoff. No journal
is appended and no driver is invoked. Fresh concrete resources may differ when
their owning session contract excludes them from semantic identity.

For restart recovery, construction and check results must belong to the exact
attempt session retained by started ownership. An operation checkpoint must
belong to both the retained effect session and exact effect attempt. Reserved
work needs no subordinate evidence. Restart evidence is therefore exact and not
substitutable even where fresh resource coordinates are flexible.

This boundary performs no journal loading, storage, reservation, execution,
reconciliation, discovery, scheduling, looping, retry selection, rollback,
compaction, garbage collection, or command action.

# DURABLE RESTART RECONCILIATION

Version 0.12.0 can reconcile one exact dispatch selected by a
*transaction_run_restart_checkpoint*. It does not discover recovery evidence or
select another unit.

A restart-classified reserved dispatch can be durably released as unstarted. A
started construction or check dispatch requires the exact caller-rehydrated
terminal result whose admitted session matches the session retained by the run.
The ordinary completion transition validates that evidence before one run
successor is committed.

A started operation requires an exact *effect_restart_checkpoint* whose attempt
is the effect attempt retained by the run and whose record is the latest record
in the supplied effect store. Automatically continuable stages are delegated to
*resume_effectful_operation*. A terminal effect record can therefore repair a
lost run completion without re-executing target mutation. A stage requiring
external resolution invokes no driver and commits no run successor.

The controller never fabricates a result from a journal identity. Construction
and check results, lifecycle and application evidence, publication evidence,
state snapshots, stores, drivers, leases, and resources remain explicit caller
authority. The reconciliation layer creates no scheduler, loop, evidence
discovery, process adoption, retry timing, rollback, compaction, garbage
collection, or effectful command.

# DURABLE TRANSACTION RUNS

Version 0.10.0 makes dispatch ownership durable without persisting semantic
package truth. A caller admits sequence zero before the first reservation; that
record contains no dispatch ownership and every positive sequence retains at
least one reservation. Sequence must equal the transition count derivable from
retained dispatch states and uncertainty observations. The caller then seals
one complete immutable snapshot after each exact ledger transition. A
reservation successor must bind the predecessor's exact
progression identity and canonical state epoch. Every record binds its
transaction, dispatch policy, caller-issued run nonce,
sequence, predecessor, run and progression identities, canonical state epoch,
terminal flags, and complete dispatch history.

Reopening requires the caller to rehydrate the exact *transaction_progress*
from authoritative construction, check, effect, and installed-state evidence.
The controller revalidates every durable graph unit and predecessor binding,
every completed dispatch against exact terminal and attempt evidence, and every
completed operation against the effect request's original expected state. A
stored identity is never promoted into semantic evidence.

The POSIX store is rooted at an explicit directory or directory descriptor. It
serializes writers with a local lock, publishes immutable read-only snapshot
files without replacement, synchronizes them and the directory, and then
atomically replaces a checksummed read-only head checkpoint followed by another
directory synchronization. The head is the physical commit point. Recovery
opens only the exact self-contained snapshot named by that head. Exact retries
before or after head publication are idempotent and require exact snapshot
bytes.

A missing, corrupt, writable, symlinked, or contradictory head or selected
snapshot fails closed. Historical snapshots are causal and audit material, not
an implicit newest-state index. The store is crash-consistent but not an
anti-rollback trust anchor. Complete snapshots are bounded to 16 MiB; this
release adds no compaction or garbage collection.

Restart assessment is conservative. A durable *reserved* dispatch can be
released. A started construction or check requires external recovery evidence.
A started operation retains the exact effect-attempt identity whose durable
journal must be inspected. *commit_operation_dispatch_start()* commits that
effect-attempt admission first and the started run snapshot second, and verifies
the exact authority returned by both stores. Only after both commits may an
effect driver run. An exact interrupted call is retryable. An orphan admission
does not promote a still-reserved dispatch into started target mutation.

The effect-attempt journal has one first-generation encoding with the same
committed-head discipline. A selected snapshot's sequence must equal the exact
transition count implied by its retained stage and evidence; lease loss cannot
hide an unresolved publication intent. A snapshot without its exact durable
head is uncommitted and fails closed. Durable run support
adds no journal discovery, semantic-evidence serialization, automatic release,
driver invocation, resource recovery, retry, rollback, or command frontend.

# TRANSACTION CHECK SESSION

A *transaction_check_request* is pure controller authority. It binds one exact
ready check node, its unique build predecessor, the successful construction
result retained by progression, and the sealed *libpkgcheck* request. It needs
no host paths or backend.

A *transaction_check_session* separately admits concrete source, checked
package, check-input, root-view, and private temporary trees together with exact
interpreter identity, numeric credentials, supplementary groups, and resource
limits. *pkgctl* delegates resource matching and execution-request construction
to *libpkgcheck-exec* and requires Version 0.1.1 or later.

Execution occurs only through an injected driver. Foreign check requests,
foreign execution requests, backend-contract failures, and thrown driver
exceptions are rejected. They are not terminal package-check evidence.

*advance_check* accepts passed or failed terminal evidence only for the same
transaction, program, check node, execution request, and retained construction.
It does not require the whole preparation-progress identity to remain unchanged;
independent ready work may advance concurrently without invalidating exact
check dependencies.

The check boundary does not select a ready unit, unpack artifacts, discover
resources, construct a platform backend, retry failures, persist sessions, or
expose an effectful command.

# OPERATION PREPARATION

Version 0.6.0 introduced preparation for one exact transaction *install*,
*upgrade*, or *remove* node. Version 0.8.0 requires that action to be ready in a
*transaction_progress* value and projects planner facts from its current state
epoch rather than the transaction's original snapshot. The caller supplies the
complete target observations, normalized package policy, target application context,
execution control, lifecycle order, and an installation reason where required.
Incoming operations additionally require one resolved runtime-closure identity
and the matching completed construction result.

The controller projects canonical installed truth through *libpkgstate-plan*.
For install and upgrade, it requires the exact transaction *build* node ordered
before the target action and consumes the complete *libpkgbuild-image* authority
retained by the matching construction result. *libpkgbuild-plan* purely projects
candidate, image, artifact, and manifest facts from that admitted authority. The
projection must retain the exact build result, archive digest, normalized image,
entry count, artifact identity, and manifest identity already established by
construction. No inspection backend is selected.

The exact *libpkgplan* operation returns either one complete package-local plan
or a typed refusal. A refusal is terminal preparation knowledge and creates no
*libpkgapply* or effect request. A successful plan is sealed into the exact
*libpkgapply* request and existing one-operation *pkgctl* effect request. Removal
never invokes incoming-artifact projection.

Preparation reads no artifact bytes and performs no archive I/O. It does not
acquire the target lease, execute lifecycle programs, apply package files, or
publish installed state. It has no command frontend and no durable preparation
journal.

# DURABLE EFFECT ATTEMPTS

Version 0.4.0 can retain the outer controller sequence as immutable journal
snapshots. One caller-issued nonce distinguishes the physical attempt while the
semantic effect-session identity remains unchanged.

The controller records admission, then an intent before every lifecycle,
application, and publication handoff, then exact terminal evidence after each
completed handoff. Every snapshot binds its predecessor, phase totals,
subordinate result identities, and any transaction, publication, or terminal
evidence. Its sequence is not advisory: it must equal the exact transition count
derivable from that retained stage and evidence.

The supplied POSIX store is anchored to an explicit directory or directory
descriptor. Encoding version one publishes read-only snapshots without
replacement and atomically advances a checksummed committed head. Operational
recovery opens only that exact head-selected snapshot. A snapshot without its
head is not promoted through directory enumeration. The store does not use an
ambient package-manager state path.

# RESTART

Restart classification is conservative. Lifecycle intent without terminal
execution evidence requires external resolution. Application intent continues
only when the caller supplies the exact durable *libpkgapply* journal. Completed
application evidence permits continuation into post lifecycle.

Publication intent is reconciled by rereading authoritative installed state.
The exact expected prior snapshot permits retry of the retained publication
request. The exact resulting snapshot permits terminal completion without a
second publication. Contradictory state or incomplete authority requires
external resolution.

A restarted attempt requires a newly held physical target-mutation lease.
*pkgctl* does not scan for application journals, infer lifecycle success, or
claim that a pre-crash lease survived.

# EFFECTFUL LIBRARY SESSION

The internal controller library can realize one exact install, upgrade, or
removal node from an already sealed transaction session. The caller supplies:

. one matching *libpkgapply* application request;
. the complete pre- and post-action lifecycle-node order;
. admitted *libpkgapply-exec* sessions for those nodes;
. one outer target-mutation lease and state projection;
. injected application, execution, and state-publication authorities.

The initial 0.3 effect boundary rejected multi-package programs, runtime
cohorts, and more than one target mutation. Version 0.6.0 admits one exact action
from the complete retained transaction program. Version 0.8.0 additionally
binds that request to the exact expected state epoch used for planning. Other
package nodes, target mutations, and runtime cohorts remain inert; this request
does not choose or execute a cross-package schedule.

# EFFECT ORDER

The controller validates one authority universe before effects begin. It then:

. executes every selected pre-action lifecycle node;
. realizes the exact application request through *libpkgapply*;
. executes every selected post-action lifecycle node;
. seals transaction evidence from all completed subordinate evidence;
. projects and publishes installed state through *libpkgstate-apply* and
  *libpkgstate*.

Publication is not attempted after lifecycle failure, incomplete application,
or loss of the outer lease before publication.

# LIFECYCLE ORDER

*libpkgtransaction* determines which lifecycle nodes occur before and after the
selected action. If peer nodes have no ordering edge, the caller supplies their
exact order. *pkgctl* validates the complete phase set and binds that order into
the effect-request identity.

An upgrade may therefore execute historical installed *pre-remove* and
*post-remove* programs together with incoming *pre-install* and *post-install*
programs around one upgrade action. No synthetic remove action is created.

# LEASE AND FAILURE KNOWLEDGE

One target-mutation lease must remain held from the first lifecycle effect
through state publication. The controller observes it before every stage and
after publication.

The result distinguishes lifecycle failure before or after application,
incomplete application, outer-lease loss, non-completed state publication,
indeterminate publication, and completion. These outcomes retain available
subordinate evidence but do not claim rollback of lifecycle side effects or
transaction-wide filesystem/state atomicity.

# TRANSACTION PROVENANCE

After all required lifecycle and application evidence succeeds, *pkgctl* seals
one transaction-evidence identity. The same identity is carried by the state
publication request and by durable installation or upgrade receipts.

# OWNED SEMANTICS

The controller owns command policy, authority-call sequencing, controller
session identities, caller-selected lifecycle ordering where the transaction
graph leaves peers unordered, outer-lease observation, transaction-evidence
composition, diagnostic ordering, and presentation.

It does not own recipe parsing, collection sealing, installed truth, dependency
selection, transaction nodes or edges, package-local plans, application
evidence, lifecycle programs, execution results, package images, or state
publication mechanics.

# COMMAND BOUNDARY

Release 0.35.0 exposes exactly one effect-implying command: *run*. *catalog*,
*resolve*, *transaction*, *inspect-run*, and *inspect-effect* remain read-only.
The command may realize the sealed multi-package transaction through the native
runtime, but only under explicit start/resume identity and one positive step
bound. There is no daemon, ambient configuration, unbounded executor, journal
discovery, implicit retry, automatic rollback, or maintenance policy.

# REPORTS

Reports are deterministic diagnostic text. They expose exact authority
identities and normalized summaries but are not authority themselves. Durable
or machine-consumed protocols require separate versioned contracts.

# CLEAN-ROOM BOUNDARY

The *pkgman* implementation is not copied, translated, or used as an internal
architecture template. Legacy behavior may be translated later by a narrow
frontend, but cannot alter the native request and session model.

# SEE ALSO

*pkgctl*(1), *libpkgfetch*(3), *libpkgbuild*(3),
*libpkgbuild-exec*(3), *libpkgplan*(3), *libpkgapply*(3),
*libpkgapply-exec*(3),
*libpkgstate*(3), *libpkgtransaction*(3), *libpkgcheck*(3),
*libpkgcheck-exec*(3)
