#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/progression.h"
source="$srcdir/src/progression.cpp"
preparation="$srcdir/src/preparation.cpp"
effect="$srcdir/src/effect.cpp"

for file in "$header" "$source" "$preparation" "$effect"; do
  [ -s "$file" ] || {
    echo "missing transaction progression authority source: $file" >&2
    exit 1
  }
done

for required in \
  'enum class transaction_node_status' \
  'enum class transaction_unit_kind' \
  'class ready_transaction_unit final' \
  'class transaction_progress final' \
  'transaction_progress begin' \
  'advance_construction' \
  'advance_check' \
  'advance_effect' \
  'pre_lifecycle_before_action' \
  'action_before_post_lifecycle' \
  'build_before_target' \
  'request.expected_state().identity()' \
  'effect publication authority is not based on the current epoch' \
  'indeterminate effect is not terminal progression evidence' \
  'progression.current_state()' \
  'construction is not retained by transaction progression'; do
  grep -F "$required" "$header" "$source" "$preparation" >/dev/null || {
    echo "missing transaction progression contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  'execute_construction(' \
  'execute_effectful_operation(' \
  'prepare_operation(' \
  'pkgapply::apply' \
  'pkgapply_exec::execute' \
  'pkgexec::execute' \
  'compare_and_publish' \
  'canonical_generation_store' \
  'libpkgexec-linux' \
  'pkgmk' \
  'pkgman'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden transaction progression shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'transaction_progress|advance_construction|advance_check|advance_effect' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'transaction progression must not acquire a command frontend' >&2
  exit 1
fi
