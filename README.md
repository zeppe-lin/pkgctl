# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.7.1 adds evidence-driven progression over one immutable transaction
program:

```text
sealed transaction session
        +
current canonical state epoch
        +
accepted construction and effect evidence
        |
        v
pending / ready / satisfied / failed / blocked nodes
        |
        v
all graph-ready construction, check, and operation units
```

A target action and its exact pre- and post-lifecycle phase nodes form one
operation unit. The unit becomes ready only when every predecessor outside the
unit is satisfied. This avoids treating the lifecycle edges inside one physical
operation as a scheduler deadlock while retaining exact status for every node.
Independent ready units remain visible together; `pkgctl` does not choose among
them.

Progression begins at the installed snapshot retained by the transaction's
resolution. Exact successful or failed construction evidence can terminate one
ready build node. Exact effect evidence can terminate one ready operation unit.
A successful effect advances the current state epoch only when its retained
`libpkgstate` publication receipt, or authoritative restart reconciliation,
proves the caller-supplied resulting snapshot. Definitive failure does not
advance state. Indeterminate publication and lost-lease results are not accepted
as terminal progression evidence.

Operation preparation now consumes a `transaction_progress` value rather than
planning against the transaction's original snapshot forever. The selected
action must be ready in the graph. Install still requires absence in the current
epoch; upgrade and removal require the historical installed authority captured
by the transaction to remain exactly current. Every sealed effect request also
retains the precise state epoch against which its application was planned, so
later stale evidence is refused.

Check nodes may become ready and are reported as check units, but this release
has no check-completion API. A separate typed check authority is required before
a scheduler can honestly advance them.

Release 0.6.0 established one-operation preparation between completed
construction and the target-effect kernel. Release 0.5.0 established one exact
package-construction session, and 0.4.0 closed the restart loop for one target
mutation sequence.

The executable still exposes only:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

There are no effect-implying CLI commands in 0.7.1. Progression executes no
source acquisition, build, check, planner, lifecycle, application, publication,
or restart authority. Ready-peer selection, parallelism, retry policy,
transaction-wide rollback, durable progression storage, and effectful command
policy remain outside this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, evidence-driven transaction progression, current-state epoch binding,
durable controller-attempt snapshots, conservative restart classification,
outer-lease observation, transaction-evidence composition, deterministic
diagnostics, and presentation.

The following meanings remain external:

- recipe and profile syntax: `libpkgsource-yaml`;
- available package universe: `libpkgcatalog` and
  `libpkgcatalog-acquire`;
- installed truth, planner projection, and publication: `libpkgstate`,
  `libpkgstate-plan`, and `libpkgstate-apply`;
- exact dependency selection: `libpkgresolve`;
- cross-package operation composition: `libpkgtransaction`;
- candidate and artifact planner projection: `libpkgsource-plan` and
  `libpkgbuild-plan`;
- package-local filesystem intent and mutation: `libpkgplan` and
  `libpkgapply`;
- lifecycle-node derivation and execution: `libpkgapply-exec` and
  `libpkgexec`;
- source acquisition and verification: `libpkgfetch`;
- build request/result and execution realization: `libpkgbuild` and
  `libpkgbuild-exec`;
- package-image authority: `libpkgimage`.

`pkgctl` does not infer transaction order beyond the exact transaction graph.
It exposes every ready unit and does not choose among ready peers. For lifecycle
nodes not ordered relative to each other by that graph, the caller supplies an
explicit order and the controller binds it into the effect request identity.

## Building

```sh
meson setup build \
  -Dlink_mode=shared \
  -Dman_pages=enabled
meson compile -C build
meson test -C build --print-errorlogs
```

Use `-Dlink_mode=static` for a static authority closure where all external
static dependencies are available.

See `DESIGN.md` for the normative controller boundary and `TESTING.md` for the
qualification contract.
