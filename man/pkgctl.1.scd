PKGCTL(1)

# NAME

pkgctl - inspect, build, or boundedly execute native package authority

# SYNOPSIS

*pkgctl* *catalog* _catalog-options_

*pkgctl* *resolve* _catalog-options_ _state-options_ _resolution-options_

*pkgctl* *transaction* _catalog-options_ _state-options_ \
_resolution-options_ [*--converge-exact*]

*pkgctl* *run* _catalog-options_ _state-options_ _resolution-options_ \
[*--converge-exact*] *--start* _identity_ _run-authority_

*pkgctl* *run* *--canonical-store* _path_ *--resume* _identity_ _run-authority_

*pkgctl* *build* _package_ [*--check*] _catalog-options_ _state-options_ \
_build-authority_ *--start* _identity_

*pkgctl* *build* *--canonical-store* _path_ *--resume* _identity_ \
_build-authority_

*pkgctl* *inspect-run* *--run-store* _path_ *--journal* _identity_

*pkgctl* *inspect-effect* *--effect-store* _path_ *--attempt* _identity_

*pkgctl* {*-V* | *-h*}

# DESCRIPTION

*pkgctl* is the Zeppe-Lin package control-plane executable.

Version 0.39.0 phase-separates concrete construction and check inputs while
retaining one sealed logical build request. Check execution realizes fresh source,
the checked package, and candidate check dependencies from retained exact owner
authority instead of borrowing construction-private staging or rediscovering why
those resources belong to the check. Start admits one complete build
policy and resume recovers it without redeclaration. Completed durable dispatches
also authorize bounded disposal of their exact private realizations; failed or
incomplete work remains available for restart or diagnosis. Native Linux preflight
requires libpkgexec-linux 0.7.1 so isolated realization is repeatable and detached
mount propagation is sealed private before attachment.

Version 0.38.0 makes durable construction and check ownership recoverable
after process death. Before either dispatch becomes durably *started*, the
controller retains its exact admitted attempt session. If terminal result evidence
is absent on resume, only that retained session may authorize replay; current
collection, session-locator, configuration, or installed-package authority does
not reconstruct the attempt. Successful construction seals and verifies its
archive below the private attempt root, retains terminal construction evidence,
and only then projects the exact retained bytes beneath *--artifact-root*. Resume
with terminal evidence completes or verifies that public projection without
rerunning construction.

Version 0.37.0 retains the native catalog, resolution, transaction, exact
durable run/effect inspection, and bounded *run* command, and adds the
construction-only *build* frontend. *build package* owns the exact package build
goal; *--check* adds its exact check goal, and catalog authority is preferred so
compatible installed state cannot satisfy a build request without producing an
archive. The command enters through the same sealed transaction and durable native
run kernel as *run* but carries no lifecycle root, target root, lifecycle
credentials, target-lock, application, or canonical-state publication authority.
Its explicit existing *--artifact-root* is public package-output authority and is
retained with the frontend kind in immutable command evidence; resume cannot
redirect it. Both *run* and *build* perform at most the positive *--max-steps*
bound. Before fresh executable work is retained or admitted, the command proves
that the selected native Linux backend can establish the execution guarantees
implied by the transaction.
Capability absence is reported as control-plane unavailability rather than a
package build/check failure. *--start* then retains the current private
command-evidence format: the complete start-only transaction inputs, exact admitted
interpreter identity, exact admitted construction/check/lifecycle backend capability
profiles, plus the original owner-encoded catalog and state snapshots. *--resume*
requires that retained command evidence and the exact admitted journal; it does not accept a second collection, target-binding, architecture,
goal, resolution-policy, or convergence request.

The command does not initialize or enumerate stores, discover journals, replan
a resumed transaction from live catalog/state, loop without a bound, sleep,
retry implicitly, schedule workers, roll back, repair, compact, or collect
history. After a successful durable terminal run, it may dispose only the exact
construction/check private realization leaves derived from retained dispatch
authority. Incomplete and failed runs authorize no such cleanup, and disposal
failure is an operational warning rather than rewritten transaction truth. Effect
restart bodies are retained in owner encodings before
controller journals reference them. A started construction also retains its
exact admitted controller session plus owner-encoded fetch/build evidence, while
a started check retains its exact admitted controller session plus owner-encoded
check evidence. Restart decodes those retained sessions without reconsulting the
fresh session locator, current construction/check configuration, or retained-
installed-package lookup. Interrupted application
recovery uses the direct *libpkgapply-posix* active-request index.

Version 0.21.0 introduced the exact durable effect-attempt inspection command
alongside the existing run-inspection command; both remain read-only sensors.

The controller library defines exact construction, check, operation,
progression, dispatch, journal, restart, and bounded-drive authorities. The
*inspect-run* and *inspect-effect* commands remain read-only sensors. *run* and
*build* are the effect-implying frontends and may realize only the already sealed
native transaction through those reviewed authorities. *build* is restricted to
construction/check authority. Neither bypasses progression,
call the single-dispatch advancement functions directly, or own package-source,
resolver, planner, application, execution, or state semantics.

Version 0.31.0 changes no command syntax. The controller library now provides
one private native construction/check session locator. It derives exact source,
predecessor or retained-installed-package resources, workspaces, package-output,
artifact, and check-temporary coordinates from sealed transaction authority,
exact progress, one run journal, one dispatch, and explicit configuration. The
locator observes and mutates no host path. Progress rehydration, operation
execution authority, final backend composition, and a mutating command remain
absent.

Version 0.30.0 changes no command syntax. Fresh construction/check execution
and restart recovery now share one deterministic admitted-session source. The
library reproduces build/check execution requests through pure adapter
projections and reacquires construction source material without invoking
effectful workspace preparation merely to recover request authority. Concrete
session and resource realization remains caller-owned.

Version 0.29.0 changes no command syntax. The library can now recover exact
construction/check controller results from retained durable evidence when a
caller supplies the complete original semantic context bodies. The evidence
store selects and validates the record; it does not discover sessions,
resources, execution requests, or backend profiles. Missing evidence remains
unresolved started ownership. The read-only command surface is unchanged.

Version 0.28.0 changes no command syntax. Construction and check execution
now publish one exact typed evidence record before terminal run retirement. The
POSIX runtime retains a fourth caller-opened directory authority for those
records. The store preserves canonical bytes and context identities but does
not reconstruct semantic results or discover missing resources.

Version 0.27.0 changes no command syntax. It corrects internal authority
composition: construction and check resources bind resolver-issued logical
inputs, and operation preparation consumes retained build/image and planner
projection authority without reopening package archives.

Version 0.26.0 changes no command syntax. The caller-configured POSIX
transaction-run runtime now receives one explicit run nonce on each launch and
owns stateless canonical dispatch-nonce derivation from the exact committed
head. It retains no random generator, nonce database, or hidden nonce cache.
Semantic, archive, backend, and canonical-state authority remain caller-owned,
and the read-only command surface is unchanged.

# COMMANDS

*catalog*
	Acquire and seal the explicitly configured collections. State and resolution
	options are invalid for this command.

*resolve*
	Acquire the catalog, open and read the exact canonical state store, resolve
	the supplied typed goals, and report exact selections and witnesses.

*transaction*
	Perform the resolve pipeline and compose the resulting immutable transaction
	program. The default convergence policy preserves all unselected installed
	packages.

*run*
	Start or resume one exact durable transaction run through the native runtime.
	The command may build, check, execute lifecycle programs, apply package
	changes, and publish installed state, but only beneath the explicit positive
	step bound and explicit authority supplied on the command line.

*build*
	Start or resume one exact construction/check transaction through that same
	native runtime. Fresh start takes one exact package subject; *--check* adds
	its check action. The frontend owns catalog preference and cannot receive
	target-operation or convergence authority. Immutable package archives are
	published only beneath the explicit *--artifact-root*.

*inspect-run*
	Open one explicitly named existing POSIX transaction-run store, inspect one
	exact journal head, and print the deterministic durable-run report. The
	command neither enumerates journals nor rehydrates package semantics.

*inspect-effect*
	Open one explicitly named existing POSIX effect-attempt store, inspect one
	exact attempt head, and print the deterministic durable-effect report. The
	command neither enumerates attempts nor traverses a transaction-run journal.

# CATALOG OPTIONS

*--collection* _name=root_
	Add one explicit collection. Repetition order defines precedence.

*--external-revision* _name=value_
	Attach diagnostic revision provenance to one named collection. It does not
	enter semantic catalog or controller-session identity.

*--max-document-bytes* _number_
	Set the positive canonical-decimal size limit for each YAML document.

# STATE OPTIONS

The following options are all required by *resolve*, *transaction*,
*run --start*, and *build --start*. Only *--canonical-store* remains a live
state option for *run --resume* and *build --resume*; the target binding is
recovered from retained command evidence.

*--canonical-store* _path_
	Open the existing canonical generation store at the absolute _path_. Missing
	stores are refused and never initialized.

*--managed-target* _identity_
	Expected managed-target identity.

*--state-store* _identity_
	Expected durable state-store identity.

*--root-view* _identity_
	Expected logical root-view identity.

*--state-backend* _identity_
	Expected state-backend identity.

*--publication-domain* _identity_
	Expected publication and locking-domain identity.

# RESOLUTION OPTIONS

*--build-architecture* _name_
	Select the exact build architecture.

*--target-architecture* _name_
	Select the exact target architecture.

*--goal* _scope=subject_
	Add one typed goal. _scope_ is *build*, *run*, *check*, or
	*lifecycle:action*. _subject_ is an exact package name or authoritative
	profile beginning with *@*. Valid lifecycle actions are *pre-install*,
	*post-install*, *pre-remove*, and *post-remove*.

*--prefer-catalog*
	Prefer the effective catalog candidate over compatible installed authority.
	The default is to retain compatible installed authority.

# BUILD OPTIONS

_package_
	Fresh *build* accepts exactly one package name. The frontend converts it to
	an exact build-scope resolver goal and always prefers catalog authority. The
	composed transaction must contain catalog-backed construction for that exact
	package; installed authority cannot silently satisfy the build verb.
	Caller-supplied *--goal*, *--prefer-catalog*, and *--converge-exact* policy
	are invalid for this command. Resume does not repeat the package subject.

*--check*
	Fresh *build* also admits the exact package check-scope goal. Build and check
	resolver scopes remain independent; this option does not replace the build
	goal. Resume does not repeat this option.

# TRANSACTION OPTIONS

*--converge-exact*
	Treat the selected target closure as complete and compose removals for other
	installed packages. This destructive policy is never enabled by default.

# RUN AND BUILD OPTIONS

Exactly one of the following is required by *run* and *build*:

*--start* _identity_
	Admit one new run with the exact lowercase SHA-256 nonce. If that run is
	already admitted, the command refuses and requires *--resume*.

*--resume* _identity_
	Resume the exact retained run nonce. The immutable command evidence and
	admitted journal must both already exist. Collection, target-binding,
	architecture, goal, build subject/check, resolution-policy, and convergence
	options are invalid on resume: their exact admitted values are decoded from
	the retained current command evidence. The current *--canonical-store* pathname remains caller authority for the
	existing physical store. Bytes outside the current private command-evidence format
	are rejected; there is no compatibility decoder or migration path.

The following construction/check authorities are required by both frontends:

*--runtime-root* _path_
	Existing absolute private runtime hierarchy. Construction/check execution
	requires pre-created *run*, *evidence*, *effects*, *command-evidence*,
	*content*, *construction-sessions*, *package-outputs*, *check-resources*, and
	*check-temporary* children. The hierarchy roots remain caller-owned; after
	successful terminal completion the frontend may prune only exact private
	dispatch leaves authorized by the durable run head and then a now-empty private
	journal container. Read-only private realization directories may be made
	owner-removable only through already-opened descriptors; the caller-owned class
	roots are not chmodded. Operation-capable
	*run* additionally requires its target-lock,
	application, lifecycle, effect-body, rejected/completed, and related private
	children. *build* does not create or open those target-operation namespaces.

*--build-root* _path_
	Existing construction/check execution root view. The command never creates or
	populates it.

*--artifact-root* _path_
	Required only by *build*. Name the existing absolute normalized public package-artifact root. It
	must be disjoint from the private runtime hierarchy and the other native
	construction-session roots. The exact coordinate is retained at fresh
	admission and must be repeated unchanged on resume.

The following target-operation authorities are required by *run* and invalid for
*build*:

*--lifecycle-root* _path_
	Existing lifecycle execution root view. It is independent from the
	construction/check root; supplying the same physical root is an explicit
	caller policy, not a controller assumption.

*--target-root* _path_
	Existing managed target root.

*--interpreter* _path_
	Interpreter coordinate. It is inspected and must match retained admission
	authority only when the current invocation can execute process work.

*--build-user-id* _number_
	Explicit numeric construction/check execution user identity.

*--build-group-id* _number_
	Explicit numeric construction/check execution primary group identity.

*--build-supplementary-group* _number_
	Add one explicit construction/check supplementary group. May be repeated.

*--lifecycle-user-id* _number_
	Explicit numeric lifecycle execution user identity.

*--lifecycle-group-id* _number_
	Explicit numeric lifecycle execution primary group identity.

*--lifecycle-supplementary-group* _number_
	Add one explicit lifecycle supplementary group. May be repeated.

The current native Linux backend admits only the invoking supervisor's numeric
credentials. For every execution scope present in the transaction, the
corresponding explicit credential set must therefore equal that supervisor
identity. Construction/check and lifecycle credentials remain separate command
authorities; *pkgctl* does not imply fakeroot, ownership virtualization, or a
credential transition that the backend cannot establish.

The following build-policy authorities are required only with *--start* and are
retained as one sealed build policy. They are invalid with *--resume*:

*--build-parallelism* _number_
	Positive construction/check parallelism selected by the operator.

*--build-source-date-epoch* _number_
	Non-negative reproducible build epoch selected by the maintainer/campaign.

The frontend fixes the remaining policy dimensions rather than exposing them as
operator knobs: file-creation mask 0022, package-root output layout, C.UTF-8
locale, UTC timezone, denied network, and isolated HOME. Resume recovers the
retained complete policy from command evidence.

*--max-steps* _number_
	Positive maximum number of controller advances in this invocation. Reaching
	the bound is successful and the same nonce may later be resumed.

*--installed-tree* _package=resource,path_
	Bind one exact retained installed-package identity and logical resource
	identity to an absolute package tree. May be repeated. No installed tree is
	discovered by scanning the target.

# RUN INSPECTION OPTIONS

*--run-store* _path_
	Open the existing caller-owned POSIX transaction-run store at _path_. The
	command never creates the directory or writer lock.

*--journal* _identity_
	Select one exact lowercase SHA-256 transaction-run journal identity.

# EFFECT INSPECTION OPTIONS

*--effect-store* _path_
	Open the existing caller-owned POSIX effect-attempt store at _path_. The
	command never creates the directory or writer lock.

*--attempt* _identity_
	Select one exact lowercase SHA-256 effect-attempt identity.

# OUTPUT

The output is a deterministic line-oriented diagnostic report containing exact
controller-session and subordinate authority identities plus normalized
summaries. *run* and *build* report the transaction, journal, committed record,
sequence, start/resume origin, drive disposition, step counts, and terminal
progress. *build* additionally reports every successful retained construction in
deterministic package/version/release order with its exact artifact path, release
and artifact identities, SHA-256, byte count, build-result identity, and
binding/image identities. Those fields report retained admitted historical
authority, not a new observation that the completed archive still exists;
terminal reporting does not rescan or reopen artifact paths. The output is not a
persistent state format or a versioned IPC protocol.

# EXIT STATUS

*0*
	The requested read-only composition completed, the run completed, or the
	explicit run step bound was reached.

*1*
	An acquisition, source, catalog, state, resolution, transaction, runtime,
	run-journal, effect-journal, backend, or controller authority refused or
	failed; this includes failed, externally blocked, or quiescent-incomplete
	runs.

*2*
	The command line is invalid or incomplete.

# SEE ALSO

*pkgctl_orchestration*(7), *pkgcatalog-scan*(1), *pkgstate-check*(1)
