# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.8.0 closes exact check sessions over transaction progression:

```text
ready transaction check unit
        +
retained successful construction evidence
        |
        v
pure transaction_check_request
        +
caller-provided concrete source/package/input/root/temp resources
        |
        v
admitted transaction_check_session
        |
        v
exact libpkgcheck-exec terminal evidence
        |
        v
advance_check() -> satisfied or failed check node
```

A pure request can be created before host paths exist. It binds one ready check
node to the exact successful construction already retained by the same
transaction progression. Concrete execution resources are admitted separately
through `libpkgcheck-exec >= 0.1.1`; missing, duplicate, forged, aliased, or
path-overlapping resources never become a controller session.

A terminal passed result satisfies only the exact check node. A terminal failed
result fails that node and blocks dependent units through ordinary graph
progression. Duplicate, cross-transaction, cross-node, stale-construction, and
mismatched execution evidence is refused. A session prepared at one progression
epoch may still complete after an unrelated ready unit advances the same
transaction, provided the exact check node remains ready and its retained
construction authority is unchanged.

The controller does not choose ready units or automatically invoke a backend.
It retains the exact transaction, check-node, construction, check-request,
execution-request, execution-result, and check-result identities across the
handoff.

Release 0.7.1 established evidence-driven transaction progression and
current-state epochs. A target action and its exact pre- and post-lifecycle
phase nodes form one operation unit, while build and check nodes remain exact
individual units. Independent ready units remain visible together; `pkgctl`
does not choose among them.

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

There are no effect-implying CLI commands in 0.8.0. The command frontend executes no source acquisition, build, check, planner,
lifecycle, application, publication, or restart authority. The internal check
controller invokes execution only through an injected driver supplied by a
library client. Ready-peer selection, parallelism, retry policy,
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
- pure check requests and terminal evidence: `libpkgcheck`;
- concrete check execution admission and realization: `libpkgcheck-exec`;
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
