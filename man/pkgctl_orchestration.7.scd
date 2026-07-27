PKGCTL_ORCHESTRATION(7)

# NAME

pkgctl_orchestration - native pkgctl authority composition

# DESCRIPTION

*pkgctl* is a controller, not a package-model authority. It decides which exact
component to call next, retains the resulting handoff identities, and presents
the read-only control state.

# AUTHORITY PIPELINE

Version 0.2.0 composes:

. explicit collection specifications through *libpkgcatalog-acquire*;
. one sealed *libpkgcatalog* snapshot;
. one existing *libpkgstate* snapshot for an exact target binding;
. one *libpkgresolve* request and result;
. one *libpkgtransaction* request and program.

The controller does not reinterpret any returned value.

# REQUESTS

A catalog request owns only explicit collection roots, precedence positions,
external diagnostic revisions, and resource limits.

A resolution request adds an existing state location, exact target binding,
build and target architectures, typed goals, and installed-authority policy.
Goals use *libpkgsource* package, profile, scope, and lifecycle values.

A transaction request adds a *libpkgtransaction* convergence policy. The native
default is *preserve-unselected*. Exact convergence must be explicit.

# SESSIONS

A catalog session binds one exact catalog snapshot identity.

A resolution session binds its catalog session, installed-state snapshot, and
resolution result.

A transaction session binds its resolution session and transaction program.

Session identities are domain-separated SHA-256 values. Filesystem paths,
command-line positions, and diagnostic revision labels are excluded. Semantic
facts are already bound by the subordinate authority identities.

# OWNED SEMANTICS

The controller owns command policy, authority-call sequencing, session binding,
diagnostic ordering, and presentation.

It does not own recipe parsing, collection sealing, installed truth, dependency
selection, requirement witnesses, transaction nodes or edges, build results,
package images, package-local plans, application evidence, or state
publication.

# READ-ONLY BOUNDARY

The canonical state store is opened with *open_existing* and read. A missing,
corrupt, mismatched, or locked store is refused. The controller never creates a
replacement empty snapshot or repairs state.

Release 0.2.0 does not fetch, build, check, inspect artifacts, plan filesystem
changes, execute lifecycle programs, mutate a target, recover an attempt, or
publish state. It therefore exposes no effect-implying package commands.

# REPORTS

Reports are deterministic diagnostic text. They expose exact authority
identities and normalized summaries but are not authority themselves. Durable
or machine-consumed protocols require separate versioned contracts.

# CLEAN-ROOM BOUNDARY

The *pkgman* implementation is not copied, translated, or used as an internal
architecture template. Legacy configuration and command behavior may be
translated later by a narrow frontend, but cannot alter the native request and
session model.

# SEE ALSO

*pkgctl*(1), *libpkgresolve*(3), *libpkgtransaction*(3)
