#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header=$srcdir/include/pkgctl/target_observation.h
source=$srcdir/src/target_observation.cpp
cli=$srcdir/cli/run_command.cpp
meson=$srcdir/src/meson.build
tests_meson=$srcdir/tests/meson.build
pipeline=$srcdir/tests/integration/package_pipeline_test.cpp

for file in "$header" "$source" "$cli" "$meson" "$tests_meson" "$pipeline"; do
  [ -s "$file" ] || {
    echo "missing target-observation authority source: $file" >&2
    exit 1
  }
done

for required in \
  'observe_native_target_paths(' \
  'pkgctl/native-target-observations/1' \
  'application_target_observer& observer' \
  'planner_observation(' \
  'record.transaction() != progress.transaction().identity()' \
  'dispatch.reserved_from_progress() != progress.identity()' \
  'dispatch.reserved_state() != progress.current_state().identity()' \
  'transaction_dispatch_state::reserved'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "target-observation core omits authority: $required" >&2
    exit 1
  }
done

grep -F "'target_observation.cpp'" "$meson" >/dev/null || {
  echo 'pkgctl-core does not compile target-observation authority' >&2
  exit 1
}
grep -F "'target_observation.h'" "$tests_meson" >/dev/null || {
  echo 'target-observation public header lacks standalone qualification' >&2
  exit 1
}
grep -F 'observe_native_target_paths(' "$cli" >/dev/null || {
  echo 'CLI does not consume pkgctl-core target-observation authority' >&2
  exit 1
}
grep -F 'pkgctl::observe_native_target_paths(' "$pipeline" >/dev/null || {
  echo 'package pipeline bypasses pkgctl-core target-observation authority' >&2
  exit 1
}

for forbidden in \
  'planner_observation(' \
  'planner_kind(' \
  'pkgctl/native-target-observations/1'; do
  if grep -F "$forbidden" "$cli" >/dev/null 2>&1; then
    echo "CLI still owns target-observation semantics: $forbidden" >&2
    exit 1
  fi
done

count=$(grep -R -F 'pkgctl/native-target-observations/1' \
  "$srcdir/src" "$srcdir/cli" | wc -l | tr -d ' ')
[ "$count" -eq 1 ] || {
  echo "target-observation identity domain has $count production owners" >&2
  exit 1
}
