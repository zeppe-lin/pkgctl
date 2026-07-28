# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.3.0 retains the read-only command pipeline from 0.2.0 and adds the
first effectful controller-library boundary for one already-authorized package
operation:

```text
exact transaction session
        +
exact libpkgapply request
        +
caller-selected lifecycle order
        +
one outer target-mutation lease
        |
        v
pre lifecycle through libpkgapply-exec
        |
        v
filesystem transition through libpkgapply
        |
        v
post lifecycle through libpkgapply-exec
        |
        v
provenance-bearing publication through libpkgstate
```

The effectful session supports one exact install, upgrade, or removal node for
one package. It retains subordinate lifecycle, application, transaction, and
state-publication evidence. It does not promise rollback of lifecycle side
effects or transaction-wide filesystem/state atomicity.

The executable still exposes only:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

There are no effect-implying CLI commands in 0.3.0. The controller library
requires callers to construct exact planner, application, lifecycle-session,
lease, backend, and state-publication authorities explicitly.

Release 0.2.0 established the read-only catalog, resolution, and transaction
command pipeline retained by this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, outer-lease observation, transaction-evidence composition, deterministic
diagnostics, and presentation.

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
