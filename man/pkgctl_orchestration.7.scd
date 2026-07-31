PKGCTL_ORCHESTRATION(7)

# NAME

pkgctl_orchestration - native pkgctl authority composition

# DESCRIPTION

*pkgctl* is a controller, not a package-model authority. It decides which exact
component to call next, retains the resulting handoff identities, and reports
the resulting control state.

# READ-ONLY PIPELINE

Version 0.11.0 retains the read-only command pipeline introduced in 0.2.0:

. explicit collection specifications through *libpkgcatalog-acquire*;
. one sealed *libpkgcatalog* snapshot;
. one existing *libpkgstate* snapshot for an exact target binding;
. one *libpkgresolve* request and result;
. one *libpkgtransaction* request and program.

The canonical state store is opened with *open_existing* for these commands.
The command frontend does not initialize or publish state.

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
in the current canonical state. *libpkgtransaction* Version 2.1.0 or later orders
both build- and check-scoped package inputs before the construction that seals
them.

The only containment policy in this release stops new reservations and starting
merely reserved work after terminal failure. Already-started independent work
may still submit exact terminal evidence. The pure dispatch model creates no threads, invokes no driver, builds no
backend, allocates no resource path, persists no run, and exposes no
effect-implying command.

# SINGLE-DISPATCH EXECUTION

Version 0.11.0 can realize one exact caller-selected dispatch. It does not select
or reserve work. The caller supplies the current committed run record, its exact
rehydrated run, the retained dispatch, one admitted session, an injected driver,
and explicit stores.

Construction and check start by appending the exact *started* run successor.
Only after that append returns the same committed authority may the driver run.
Returned evidence is submitted through the ordinary completion function and the
resulting successor is appended. A failed start append invokes no driver. Driver
escape or a failed final append leaves the started dispatch selected for exact
recovery.

Operation start first appends effect-attempt admission, then appends the started
run retaining that attempt identity. Both commits precede every physical effect.
Execution continues through the durable effect journal. Successful publication
is paired with a fresh installed-state read before run completion. Lost lease or
indeterminate publication remains an observation on active ownership. A terminal
effect journal whose run completion was not committed is recovered by inspecting
the retained effect-attempt identity.

This layer creates no scheduler, loop, thread, backend, resource-discovery rule,
retry rule, rollback, automatic release, journal discovery, or effectful command.

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

Version 0.9.1 established the same committed-head discipline for effect-attempt
encoding version two. A selected snapshot's sequence must equal the exact
transition count implied by its retained stage and evidence; lease loss cannot
hide an unresolved publication intent. Strict version-one record-only histories
remain readable through semantic-chain validation. Appending a successor
establishes a version-two head; exact retry of a legacy latest record first
atomically rewrites the selected snapshot as version two and only then publishes
its head. Durable run support
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
before the target action, reinspects the retained artifact through an injected
*libpkgimage* backend, and projects candidate, image, and artifact facts through
*libpkgbuild-plan*. The archive digest, normalized image identity, and entry
count must reproduce construction evidence. Inspection backend identity may
differ.

The exact *libpkgplan* operation returns either one complete package-local plan
or a typed refusal. A refusal is terminal preparation knowledge and creates no
*libpkgapply* or effect request. A successful plan is sealed into the exact
*libpkgapply* request and existing one-operation *pkgctl* effect request. Removal
never invokes incoming-artifact projection.

Preparation may read exact artifact bytes, but it does not acquire the target
lease, execute lifecycle programs, apply package files, or publish installed
state. It has no command frontend and no durable preparation journal.

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
descriptor. Encoding version two publishes read-only snapshots without
replacement and atomically advances a checksummed committed head. Operational
recovery opens only that exact head-selected snapshot. Strict version-one
record-only histories remain readable by full chain validation and are upgraded
by the first exact version-two append. The store does not use an ambient
package-manager state path.

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

Release 0.8.0 exposes no effect-implying command. *catalog*, *resolve*, and
*transaction* remain read-only. Install, update, removal, multi-package
scheduling, recursive construction, planning, and recovery policy require
later explicit command contracts.

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
