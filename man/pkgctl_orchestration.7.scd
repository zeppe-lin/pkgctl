PKGCTL_ORCHESTRATION(7)

# NAME

pkgctl_orchestration - native pkgctl authority composition

# DESCRIPTION

*pkgctl* is a controller, not a package-model authority. It decides which exact
component to call next, retains the resulting handoff identities, and reports
the resulting control state.

# READ-ONLY PIPELINE

Version 0.7.0 retains the read-only command pipeline introduced in 0.2.0:

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

Version 0.7.0 retains immutable controller knowledge for one transaction
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

Check units can become ready, but Version 0.7.0 defines no check-completion API.
No current authority seals a check request and terminal check result.

# OPERATION PREPARATION

Version 0.6.0 introduced preparation for one exact transaction *install*,
*upgrade*, or *remove* node. Version 0.7.0 requires that action to be ready in a
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
completed handoff. Every snapshot binds its predecessor, sequence number,
phase totals, subordinate result identities, and any transaction, publication,
or terminal evidence.

The supplied POSIX store is anchored to an explicit directory or directory
descriptor. It publishes read-only snapshots without replacement and validates
record identity, predecessor linkage, and encoded causal shape. It does not use
an ambient package-manager state path.

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
from the complete retained transaction program. Version 0.7.0 additionally
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

Release 0.7.0 exposes no effect-implying command. *catalog*, *resolve*, and
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
*libpkgstate*(3), *libpkgtransaction*(3)
