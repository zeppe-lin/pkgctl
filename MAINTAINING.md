# Maintaining pkgctl

## Authority review

Before accepting a feature, identify which component owns every fact consumed
or produced by the change. Reject changes that make `pkgctl` parse, infer, or
recompute an authority value already owned by another library.

## Dependency direction

Core orchestration values remain independent of source, build, image, planner,
application, and state libraries. Concrete adapters depend inward on the core
and outward on exactly the authorities they compose.

An adapter must not add its dependencies to unrelated core pkg-config or header
closures.

## Compatibility policy

`pkgman` compatibility is maintained through explicit translation and migration
tests. Native semantics are not weakened to preserve accidental behavior.

## Release procedure

1. run the complete GCC and Clang qualification suites;
2. verify every patch boundary when publishing a series;
3. update release metadata and manuals together;
4. inspect linkage and installed metadata once external dependencies exist;
5. tag signed releases only from a clean tree.
