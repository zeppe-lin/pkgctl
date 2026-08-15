#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"
test_source="$srcdir/tests/integration/cli_build_process_death_test.sh"
run_head="$srcdir/tests/fixtures/run_head_interrupt_fixture.cpp"
artifact="$srcdir/tests/fixtures/artifact_publication_interrupt_fixture.cpp"
readme="$srcdir/tests/fixtures/README.md"

for path in "$meson" "$test_source" "$run_head" "$artifact" "$readme"; do
  [ -s "$path" ] || {
    echo "process-death qualification source is absent: $path" >&2
    exit 1
  }
done

for required in \
  'construction-started' \
  'artifact-published' \
  'check-started' \
  'run-head-interrupt-fixture' \
  'artifact-publication-interrupt-fixture' \
  "suite: 'integration-privileged'"; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "process-death Meson qualification omits: $required" >&2
    exit 1
  }
done

for required in \
  'head_magic' \
  "'P', 'K', 'G', 'R', 'U', 'N', 'H', '1'" \
  'head_has_sequence' \
  'SYS_fsync' \
  'PTRACE_SYSCALL' \
  'SIGKILL'; do
  grep -F -- "$required" "$run_head" >/dev/null || {
    echo "run-head interruption fixture omits: $required" >&2
    exit 1
  }
done

for required in \
  'published_artifact_exists' \
  'recursive_directory_iterator' \
  'extension() == ".tar"' \
  'find(".tmp.")' \
  'PTRACE_SYSCALL' \
  'SIGKILL'; do
  grep -F -- "$required" "$artifact" >/dev/null || {
    echo "artifact interruption fixture omits: $required" >&2
    exit 1
  }
done

for required in \
  'build tool --check' \
  '"$runtime/run" 2 --' \
  '"$runtime/run" 8 --' \
  '"$artifact_interrupt_fixture" "$artifacts" --' \
  '--resume "$nonce"' \
  'origin resumed' \
  'disposition completed' \
  'complete yes' \
  'failed no' \
  'artifacts 2' \
  'check-payload checked:tool-source+dependency-source'; do
  grep -F -- "$required" "$test_source" >/dev/null || {
    echo "process-death CLI proof omits: $required" >&2
    exit 1
  }
done

for required in \
  '`run_head_interrupt_fixture.cpp`' \
  '`artifact_publication_interrupt_fixture.cpp`'; do
  grep -F -- "$required" "$readme" >/dev/null || {
    echo "fixture documentation omits: $required" >&2
    exit 1
  }
done
