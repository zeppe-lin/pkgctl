# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.5.0 adds the first package-construction library session while
retaining the durable target-effect kernel from 0.4.0:

```text
exact transaction build node
        +
verified package-input tree identities
        +
explicit source/store and build coordinates
        |
        v
libpkgfetch source materialization
        |
        v
sealed libpkgbuild request
        |
        v
libpkgbuild-exec execution and independent artifact inspection
```

The controller validates one exact catalog-backed build node before effects. It
requires every package input to match the resolver's exact dependency selection
and binds the source snapshot, input-set identity, resolver-selected architectures,
build policy, acquisition bounds, logical root view, interpreter, and numeric
credentials. Source and store roots, dependency-tree paths, workspace paths,
and artifact destinations remain call-scoped effect coordinates.

Construction does not recursively schedule dependencies. The caller supplies
every exact package-input subject, tree identity, and host tree. `pkgctl` invokes
an injected backend-neutral construction driver, retains the complete verified
source materialization and build-execution result, and promotes completion only
when `libpkgbuild-exec` returns a successful build with independent archive
inspection evidence.

Release 0.4.0 closed the restart loop for the separate one-operation target
mutation sequence. That durable effect journal remains unchanged. Construction
attempts are not restart journals in 0.5.0: an interrupted build is not inferred
successful or replayed automatically, and no installed state is published from
a construction result.

The executable still exposes only:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

There are no effect-implying CLI commands in 0.5.0. The construction API is a
controller-library boundary for callers that already possess exact dependency
trees, execution resources, and backend authority.

Release 0.3.0 established the one-operation effectful session. Release 0.2.0
established the read-only catalog, resolution, and transaction command pipeline
retained by this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, durable controller-attempt snapshots, conservative restart
classification, outer-lease observation, transaction-evidence composition,
deterministic diagnostics, and presentation.

The following meanings remain external:

- recipe and profile syntax: `libpkgsource-yaml`;
- available package universe: `libpkgcatalog` and
  `libpkgcatalog-acquire`;
- installed truth and publication: `libpkgstate` and
  `libpkgstate-apply`;
- exact dependency selection: `libpkgresolve`;
- cross-package operation composition: `libpkgtransaction`;
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
