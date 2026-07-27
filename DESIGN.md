# pkgctl design

## Purpose

`pkgctl` is the Zeppe-Lin controller. It selects which authoritative component
to call next, retains the exact handoff identities, and reports the resulting
control state. It does not recreate the facts or decisions owned by those
components.

The central invariant is:

> A controller session is evidence that exact authorities were composed; it is
> not another package-source, resolver, transaction, planner, or state model.

## Release 0.2.0 boundary

The read-only pipeline is:

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

The controller never initializes a missing state store. The state pathname and
all five target-binding identities are supplied explicitly. A binding mismatch,
missing store, corrupt generation, or locked store is a state-authority failure.

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
9. controller session identities;
10. deterministic line-oriented presentation.

Package and profile identities are `libpkgsource` values. Catalog candidates are
`libpkgcatalog` values. Installed records are `libpkgstate` values. Selections
and witnesses are `libpkgresolve` values. Operation nodes, edges, cohorts, and
convergence semantics are `libpkgtransaction` values.

## Requests

`catalog_request` contains explicit acquisition specifications and document
limits. It does not infer roots, collection names, revisions, or precedence.

`resolution_request` adds one existing canonical state location, architecture
context, typed goals, and resolver policy. Goals are normalized through the
resolver's exact scope and subject values. Duplicate semantic goals are
rejected even when their diagnostic origins differ.

`transaction_request` adds one native convergence policy. The default is
`preserve_unselected`. `converge_exact` is accepted only through an explicit CLI
flag and means the caller supplied the complete desired target closure.

## Sessions and identities

A catalog session retains the explicit acquisition request and the resulting
sealed catalog snapshot. Its identity binds the exact catalog identity.

A resolution session retains the catalog session, one exact installed-state
snapshot, and one exact resolution result. Its identity binds all three exact
identities.

A transaction session retains the resolution session and one exact transaction
program. Its identity binds the resolution-session and transaction-program
identities.

Session identities use domain-separated SHA-256. Filesystem paths, command-line
positions, collection declaration provenance, and external revision labels are
not semantic authority and do not enter session identity. The sealed authority
results already bind the normalized facts that matter.

## Presentation

Reports are deterministic line-oriented diagnostics. They expose exact session,
catalog, installed-state, resolution, and transaction identities plus normalized
candidate, selection, edge, goal, node, and cohort summaries.

The report format is not a persistent catalog, state, resolver, transaction, or
IPC protocol. A machine protocol must receive its own versioned contract rather
than treating diagnostic text as authority.

## Non-authorities

Release 0.2.0 does not:

- read historical `pkgman.conf`;
- parse Pkgfile or a historical package database;
- infer package identity from directory or archive names;
- select dependency candidates itself;
- compose package-operation graphs itself;
- initialize or publish installed state;
- fetch, build, or test sources;
- inspect or create package artifacts;
- construct `libpkgplan` requests;
- execute lifecycle programs;
- apply filesystem mutations;
- promise transaction-wide atomicity or rollback.

## Clean-room rule

The project remains original Zeppe-Lin code. Public legacy commands and observed
behavior may inform a future migration frontend, but native controller types and
control flow do not inherit `pkgman`, CRUX `prt-get`, or `pkgmk` internals.

Compatibility translation must remain outside the native request and session
model.
