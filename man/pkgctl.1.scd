PKGCTL(1)

# NAME

pkgctl - inspect native package resolution and transaction authority

# SYNOPSIS

*pkgctl* *catalog* _catalog-options_

*pkgctl* *resolve* _catalog-options_ _state-options_ _resolution-options_

*pkgctl* *transaction* _catalog-options_ _state-options_
_resolution-options_ [*--converge-exact*]

*pkgctl* *inspect-run* *--run-store* _path_ *--journal* _identity_

*pkgctl* *inspect-effect* *--effect-store* _path_ *--attempt* _identity_

*pkgctl* {*-V* | *-h*}

# DESCRIPTION

*pkgctl* is the Zeppe-Lin package control-plane executable.

Version 0.30.0 retains the native catalog, resolution, transaction, and exact
durable run/effect inspection commands. The command surface remains read-only.
The controller library now persists exact construction/check execution evidence
between started dispatch ownership and terminal run retirement, but no command
creates, discovers, or rehydrates those records.

Version 0.21.0 introduced the exact durable effect-attempt inspection command
alongside the existing run-inspection command; both remain read-only sensors.

The controller library also defines exact one-candidate construction,
transaction progression, immutable dispatch ownership, durable transaction-run
journals, one-dispatch write-ahead execution, exact check
request/session/result, one-operation preparation, single-operation effect,
conservative restart, one-dispatch restart reconciliation, exact
caller-supplied run-authority handoffs, durable initial run admission,
restart-safe bounded transaction launch, read-only durable run and
effect-attempt inspection, one bounded transaction-run advancement call, and
bounded serial driving. The *inspect-run* and *inspect-effect* commands expose
only the existing durable sensors; no effect-implying command is exposed.
Progression reports every graph-ready construction, check, or operation unit.
Library clients may advance one durable journal by reconciling retained ownership
or, only when the run is quiescent, reserving and realizing one canonical
policy-allowed unit through injected authorities, drivers, and stores. No
scheduler, worker, unbounded executor, or mutating command is provided.
Preparation consumes retained build/image admission and the pure
build-plan projection to seal package-local plan/application handoffs when
called by a library client; it does not reopen artifact bytes. All
executable commands remain read-only: they never initialize or publish state,
materialize sources, build packages, inspect artifacts, plan package changes,
execute lifecycle programs, or apply mutations.

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

The following options are all required by *resolve* and *transaction*:

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

# TRANSACTION OPTIONS

*--converge-exact*
	Treat the selected target closure as complete and compose removals for other
	installed packages. This destructive policy is never enabled by default.

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
summaries. It is not a persistent state format or a versioned IPC protocol.

# EXIT STATUS

*0*
	The requested read-only authority composition completed.

*1*
	An acquisition, source, catalog, state, resolution, transaction, run-journal,
	effect-journal, or controller authority refused or failed.

*2*
	The command line is invalid or incomplete.

# SEE ALSO

*pkgctl_orchestration*(7), *pkgcatalog-scan*(1), *pkgstate-check*(1)
