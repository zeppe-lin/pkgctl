# pkgctl

`pkgctl` is the Zeppe-Lin package transaction orchestrator.

The implementation is original C++17 code written for Zeppe-Lin. It is not
derived from `pkgman`, CRUX `prt-get`, or another package manager. The project
is licensed under GPL-3.0-or-later and is copyright Alexandr Savca.

Release 0.1.0 establishes the orchestration model only. The executable supports
`--help` and `--version`; package transaction commands deliberately fail until
their authority composition is implemented and qualified.

## Authority

`pkgctl` owns:

- user intent and transaction policy;
- typed resolver constraints;
- immutable multi-package operation graphs;
- sequencing of source, build, image, planning, application, and state
  authorities;
- transaction progress, diagnostics, and recovery routing.

`pkgctl` does not own package-source interpretation, package building, archive
inspection, one-package transition policy, filesystem application, or canonical
installed state. Those authorities belong to the dedicated Zeppe-Lin libraries.

The intended dependency graph is:

```text
libpkgsource -> libpkgbuild
libpkgimage
libpkgstate -> libpkgstate-plan
libpkgplan
libpkgapply
libpkgstate -> libpkgstate-apply
                 ^
                 |
               pkgctl
```

The first release does not link any of these libraries. Adapters will be added
in dependency order after the orchestration kernel is frozen.

## Clean-room boundary

The old `pkgman` codebase is a compatibility specimen, not a source substrate.
Its command names, configuration, documented behavior, and observed failures
may inform migration tests. Its implementation, internal types, control flow,
and naming are not copied or translated.

No `pkgmk` exit code, `pkgmk.conf` layout rule, archive filename convention, or
legacy package-database grammar is part of the native `pkgctl` model.

## Building

With Meson:

```sh
meson setup build
meson compile -C build
meson test -C build
```

The test suite can also be compiled directly with a C++17 compiler through
`tests/run-direct.sh` once the model commits are present.

See `DESIGN.md` for the normative authority graph and `TESTING.md` for the
qualification contract.
