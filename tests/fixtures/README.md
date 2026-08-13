# pkgctl Test Fixtures

This directory contains deterministic fiction used only to qualify the
operator-facing executable boundary.

`collections/simple-install` is the smallest native install transaction: one
source package, no dependencies, no sources, no checks, and no lifecycle
programs. Its payload is `/usr/bin/pkgctl-fixture` with fixed bytes.

`collections/native-construction` is the process-reality construction fixture.
`dep` and `tool` each carry one digest-pinned local source; `tool` has a build
requirement on `dep` and a check program. The campaign requests both build and
check resolver scopes explicitly, so each durable input/action is backed by its
own admitted resolver authority. `dep` uses the one explicitly provisioned
`chmod` runtime to seal an executable source-derived `dep-tool`; `tool` then
executes that program directly from its read-only build-input tree. Successful
output can exist only if the production adapters mounted executable package
inputs plus the fetched source, predecessor package tree, constructed package
tree, and check source at their declared logical paths.

`collections/lifecycle-pre-install` and
`collections/lifecycle-post-install` keep that same payload while adding exactly
one lifecycle declaration on the named side of application. They exist only to
qualify controller restart authority at a durable lifecycle intent; the process
is interrupted before either lifecycle program is entered.

`native_interpreter_x86_64.S` is a test-only static Linux x86-64 executable. It
validates the `INTERPRETER -c PROGRAM pkgexec` invocation made by
`libpkgexec-linux` and writes exactly the payload declared by the fixture
recipe. It is deliberately not a shell implementation and is never installed.
Its purpose is to exercise the production isolation/build path without copying
a host shell, dynamic loader, or shared libraries into the synthetic build
root.

The C++ fixture executables create or inspect owner-defined durable formats;
they do not duplicate those formats in shell.

`native_target_lock_holder.cpp` is the one control-plane contention fixture.
It derives the exact native-command mutation-exclusion domain from the
read-only resolution target-binding and transaction identities plus the
selected command roots, then acquires the real `libpkgapply-posix` nonblocking
mutation lease. It does not emulate `flock(2)`, inject errno, or call the controller.
The held lease is released only when the test explicitly terminates the fixture.

`native_root_view_fixture.sh` creates only the stable logical mount
destinations required by the native build, check, and lifecycle adapters. The
root view itself remains caller-owned fiction: the production Linux backend is
expected to reject missing destinations rather than silently populate them.
Scenario-specific dependency input leaves are not invented by this fixture.


`native_runtime_root_fixture.cpp` adds explicitly selected real executables
to such a caller-owned root view. It canonicalizes `/bin/sh`, copies that exact
ELF executable plus its requested logical spelling, and may copy additional
caller-named executable runtimes. Each executable brings only its program
interpreter and `ldd`-reported shared-library closure. The fixture reports the
canonical interpreter path used by `pkgctl`; it never copies an ambient command
set or filesystem tree.
