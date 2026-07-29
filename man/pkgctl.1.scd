PKGCTL(1)

# NAME

pkgctl - inspect native package resolution and transaction authority

# SYNOPSIS

*pkgctl* *catalog* _catalog-options_

*pkgctl* *resolve* _catalog-options_ _state-options_ _resolution-options_

*pkgctl* *transaction* _catalog-options_ _state-options_ _resolution-options_ [*--converge-exact*]

*pkgctl* {*-V* | *-h*}

# DESCRIPTION

*pkgctl* is the Zeppe-Lin package control-plane executable.

Version 0.6.0 acquires explicit native collections, reads one existing canonical
installed-state generation, resolves typed package requirements, composes one
immutable cross-package transaction program, and prints deterministic reports.

The controller library also defines exact one-candidate construction,
one-operation preparation, single-operation effect, and durable restart
sessions, but no effect-implying command is exposed. Preparation can inspect
exact artifact bytes and seal package-local plan/application handoffs when called
by a library client. All executable commands remain read-only: they never
initialize or publish state, materialize sources, build packages, inspect
artifacts, plan package changes, execute lifecycle programs, or apply mutations.

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

# OUTPUT

The output is a deterministic line-oriented diagnostic report containing exact
controller-session and subordinate authority identities plus normalized
summaries. It is not a persistent state format or a versioned IPC protocol.

# EXIT STATUS

*0*
	The requested read-only authority composition completed.

*1*
	An acquisition, source, catalog, state, resolution, transaction, or
	controller authority refused or failed.

*2*
	The command line is invalid or incomplete.

# SEE ALSO

*pkgctl_orchestration*(7), *pkgcatalog-scan*(1), *pkgstate-check*(1)
