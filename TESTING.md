# pkgctl testing

The test suite protects orchestration semantics rather than implementation
shape.

## Required qualification

Every release must establish:

- C++17 compilation under GCC and Clang with warnings as errors;
- independent usability of every internal public header;
- intent and constraint validation;
- operation-graph normalization and cycle rejection;
- deterministic execution order independent of insertion order;
- native outcome invariants without backend exit-code leakage;
- CLI status and diagnostic contracts;
- release metadata consistency;
- clean `git diff --check`, `git show --check`, and `git fsck` results.

## Clean-room regression

Source and build files are checked for prohibited legacy implementation tokens.
The check is intentionally narrow: documentation may name `pkgman` while
explaining migration, but native headers and source must not import its internal
vocabulary.

## Future integration tests

Each authority adapter must be tested against exact repository source trees or
installed releases. Stubs may test local control flow but cannot qualify an
adapter release.

End-to-end tests must retain exact identities for source snapshots, build
artifacts, package images, plans, application attempts, completed evidence,
state publication requests, and publication receipts.
