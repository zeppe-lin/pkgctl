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
  'integration/cli_readonly_test.sh' \
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
