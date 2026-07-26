PKGCTL_ORCHESTRATION(7)

# NAME

pkgctl_orchestration - pkgctl authority and transaction model

# DESCRIPTION

pkgctl coordinates independent package authorities. It decides which authority
to call next but does not recreate that authority's facts or decisions.

# OWNED SEMANTICS

The native pkgctl model owns:

. user intent;
. typed resolver and execution constraints;
. immutable multi-package operation graphs;
. dependency-safe execution ordering;
. continuation and fail-fast policy;
. orchestrator-native step outcomes;
. transaction progress and recovery routing;
. stable diagnostics and presentation.

# EXTERNAL AUTHORITIES

The following meanings remain external:

. source capture and normalized build descriptions: libpkgsource;
. build execution and exact artifact results: libpkgbuild;
. archive inspection and normalized package images: libpkgimage;
. canonical installed state: libpkgstate;
. state projection into planner facts: libpkgstate-plan;
. one-package transition decisions: libpkgplan;
. physical application and recovery evidence: libpkgapply;
. completed application projection into state publication:
  libpkgstate-apply.

The pkgctl core does not depend on these libraries. Concrete adapters are added
in dependency order and must not contaminate unrelated core headers or metadata.

# INTENT

Native user intent is a closed set:

. install named packages;
. update named installed packages;
. remove named installed packages;
. update every eligible installed package;
. download named package inputs without installation.

Target lists are canonical sets. Empty targeted requests and duplicate targets
are invalid.

# CONSTRAINTS

Constraint classes are separate semantic axes:

. exclude-target removes a package from the final execution target set;
. forbid-node prevents a package from appearing in the resolved graph;
. prune-subtree prevents traversal through one dependency subtree;
. hold-installed-release prevents replacement of one installed release;
. require-candidate requires an eligible source candidate.

Different classes may name the same package. Exact duplicate typed constraints
are invalid. No single ignore bit represents all classes.

# OPERATION GRAPH

Selection produces an immutable directed acyclic graph. Every operation has a
stable local identity, one package, one transition kind, and zero or more exact
prerequisite operation identities.

Construction rejects:

. duplicate operation identities;
. multiple operations targeting one package;
. duplicate prerequisites;
. self-dependencies;
. absent prerequisites;
. dependency cycles.

Execution order is a deterministic topological order. Among simultaneously
ready operations, lexical operation identity determines order. Caller insertion
order has no semantic effect.

# OUTCOMES

One step ends as succeeded, refused, failed, or skipped. Refusal is a complete
policy or authority decision. Failure is an attempted step that did not
complete. Skip means the step was deliberately not attempted.

Failure domains are orchestrator-native. Raw process exits, signals, and backend
status tables remain in concrete adapters and diagnostic evidence; they are not
core transaction result values.

# CLEAN-ROOM BOUNDARY

pkgctl is original Zeppe-Lin code. The pkgman implementation is not copied,
translated, or used as an internal architecture template. Compatibility work may
observe public interfaces and deployed behavior, but native pkgctl types do not
inherit pkgman internals.

# RELEASE 0.1.0

Version 0.1.0 exposes the model and help/version CLI only. Transaction commands
are intentionally unavailable until exact authority adapters and recovery paths
are implemented and qualified.

# SEE ALSO

*pkgctl*(1)
