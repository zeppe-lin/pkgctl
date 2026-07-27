# pkgctl

`pkgctl` is the Zeppe-Lin package control-plane executable.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.2.0 provides a read-only native control loop:

```text
explicit collection and target inputs
        ↓
libpkgcatalog-acquire
        ↓
libpkgstate snapshot
        ↓
libpkgresolve result
        ↓
libpkgtransaction program
        ↓
deterministic report
```

The supported commands are:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

Release 0.2.0 is deliberately read-only. It does not initialize state, fetch or
build sources, inspect package artifacts, construct package-local filesystem
plans, execute lifecycle programs, apply mutations, publish state, or expose
`install`, `update`, `remove`, or `sysup` commands.

## Authority

`pkgctl` owns only command policy, authority-call sequencing, read-only session
binding, deterministic diagnostics, and presentation.

The following meanings remain external:

- recipe and profile syntax: `libpkgsource-yaml`;
- available package universe: `libpkgcatalog` and
  `libpkgcatalog-acquire`;
- installed truth: `libpkgstate`;
- exact dependency selection: `libpkgresolve`;
- cross-package operation composition: `libpkgtransaction`;
- later build, image, package-local plan, application, and publication effects:
  their dedicated authorities.

The provisional 0.1.0 intent, constraint, outcome, and operation-graph types are
not retained. Their meanings are now owned by the native resolver and
transaction libraries.

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
