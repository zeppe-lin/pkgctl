#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
meson="$srcdir/tests/meson.build"

for directory in unit support fixtures integration contracts; do
  [ -d "$srcdir/tests/$directory" ] || {
    echo "missing test qualification role directory: $directory" >&2
    exit 1
  }
done

for misplaced in "$srcdir"/tests/*.cpp "$srcdir"/tests/*.h \
                 "$srcdir"/tests/check_*.sh; do
  [ ! -e "$misplaced" ] || {
    echo "test source escaped its qualification role: $misplaced" >&2
    exit 1
  }
done

for suite in unit integration contract header; do
  grep -F "suite: '$suite'" "$meson" >/dev/null || {
    echo "Meson omits test qualification suite: $suite" >&2
    exit 1
  }
done

for path in \
  'unit/construction_test.cpp' \
  'fixtures/state_fixture.cpp' \
  'fixtures/native_root_view_fixture.sh' \
  'fixtures/application_intent_interrupt_fixture.cpp' \
  'fixtures/application_intent_interrupt_probe.cpp' \
  'fixtures/publication_intent_interrupt_fixture.cpp' \
  'fixtures/publication_intent_interrupt_probe.cpp' \
  'fixtures/publication_terminal_interrupt_fixture.cpp' \
  'fixtures/publication_terminal_interrupt_probe.cpp' \
  'integration/application_intent_interrupt_fixture_test.sh' \
  'integration/publication_intent_interrupt_fixture_test.sh' \
  'integration/publication_terminal_interrupt_fixture_test.sh' \
  'integration/cli_readonly_test.sh' \
  'integration/cli_run_removal_test.sh' \
  'integration/cli_run_application_restart_test.sh' \
  'integration/cli_run_publication_restart_test.sh' \
  'integration/cli_run_publication_terminal_restart_test.sh' \
  'contracts/check_test_layout_contract.sh'; do
  grep -F "$path" "$meson" "$srcdir/tests/run-direct.sh" >/dev/null || {
    echo "qualification wiring omits categorized test source: $path" >&2
    exit 1
  }
done

[ -s "$srcdir/tests/support/construction_fixture.h" ] || {
  echo 'missing categorized shared construction fixture' >&2
  exit 1
}
