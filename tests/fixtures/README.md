# pkgctl Test Fixtures

This directory contains deterministic fiction used only to qualify the
operator-facing executable boundary.

`collections/simple-install` is the smallest native install transaction: one
source package, no dependencies, no sources, no checks, and no lifecycle
programs. Its payload is `/usr/bin/pkgctl-fixture` with fixed bytes.

`native_interpreter_x86_64.S` is a test-only static Linux x86-64 executable. It
validates the `INTERPRETER -c PROGRAM pkgexec` invocation made by
`libpkgexec-linux` and writes exactly the payload declared by the fixture
recipe. It is deliberately not a shell implementation and is never installed.
Its purpose is to exercise the production isolation/build path without copying
a host shell, dynamic loader, or shared libraries into the synthetic build
root.

The C++ fixture executables create or inspect owner-defined durable formats;
they do not duplicate those formats in shell.
