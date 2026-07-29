# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.6.0 adds one-operation preparation between completed construction
and the existing target-effect kernel:

```text
exact install, upgrade, or removal transaction node
        +
completed construction result for incoming operations
        +
canonical installed-state snapshot
        +
caller observations, runtime closure, policy, and lifecycle order
        |
        v
libpkgstate-plan and libpkgbuild-plan projections
        |
        v
libpkgplan success or typed refusal
        |
        v
sealed libpkgapply request and pkgctl effect request
```

For installation and upgrade, the controller requires the exact completed
construction ordered before the selected target node. It reinspects the sealed
artifact through an injected `libpkgimage` backend, requires the same archive
digest, normalized image identity, and entry count recorded by construction,
and admits one `libpkgapply` incoming-package authority. A different compatible
inspection backend may produce the evidence; backend identity is not package
truth.

For all three operation kinds, `libpkgstate-plan` projects the complete
canonical snapshot into planner facts. The caller remains authoritative for the
target observations, normalized package policy, runtime closure, target
application context, execution guarantees, and lifecycle order. `pkgctl` calls
the exact `libpkgplan` operation and retains either its complete plan or its
typed refusal. A refusal is terminal preparation knowledge: no application or
effect request is manufactured.

Preparation is read-only authority composition. Incoming preparation performs
exact-byte artifact inspection, but it does not acquire the target lease,
execute lifecycle programs, mutate package files, or publish installed state.
Removal preparation does not inspect an incoming artifact at all.

The resulting effect request retains the complete sealed transaction session but
selects only one exact target action and its exact lifecycle phase set. Other
package nodes, target actions, and runtime cohorts are not executed or ordered by
that request; cross-package scheduling remains a separate controller boundary.

Release 0.5.0 established one exact package-construction session. Release 0.4.0
closed the restart loop for the separate one-operation target mutation
sequence. Construction and preparation are not added to the durable effect
journal in 0.6.0.

The executable still exposes only:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

There are no effect-implying CLI commands in 0.6.0. Recursive construction,
check execution, cross-package scheduling, durable construction or preparation
attempts, and effectful command policy remain outside this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, durable controller-attempt snapshots, conservative restart
classification, outer-lease observation, transaction-evidence composition,
deterministic diagnostics, and presentation.

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
For lifecycle nodes not ordered relative to each other by that graph, the
caller supplies an explicit order and the controller binds it into the effect
request identity.

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
