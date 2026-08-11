#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/check.h"
source="$srcdir/src/check.cpp"
progression="$srcdir/src/progression.cpp"

for file in "$header" "$source" "$progression"; do
  [ -s "$file" ] || {
    echo "missing transaction check authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_check_request final' \
  'transaction_check_request make' \
  'class transaction_check_session final' \
  'transaction_check_session admit' \
  'class transaction_check_driver' \
  'class transaction_check_result final' \
  'execute_transaction_check' \
  'advance_check' \
  'invalid_check_session' \
  'check_driver_contract_violation' \
  'build_before_check' \
  'progress.status(check_node)' \
  'request.construction().identity()' \
  'session.execution_request()' \
  'pkgcheck_exec::admitted_check_session::admit' \
  'pkgcheck_exec::seal_execution_request'; do
  grep -F -- "$required" "$header" "$source" "$progression" >/dev/null || {
    echo "missing transaction check contract: $required" >&2
    exit 1
  }
done

if grep -F 'prepared_from_progress() != progress.identity()' \
    "$progression" >/dev/null 2>&1; then
  echo 'check completion must not reject unrelated progression advancement' >&2
  exit 1
fi

for forbidden in \
  'libpkgexec-linux' \
  'canonical_generation_store' \
  'compare_and_publish' \
  'pkgapply::apply' \
  'pkgapply_exec::execute' \
  'execute_construction(' \
  'prepare_operation(' \
  'pkgcheck_exec::prepare' \
  'ready_units().front' \
  'pkgmk' \
  'pkgman'; do
  if grep -F -- "$forbidden" "$header" "$source" "$progression" >/dev/null 2>&1; then
    echo "forbidden transaction check shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'transaction_check_request|transaction_check_session|advance_check' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'transaction check authority must not acquire a command frontend' >&2
  exit 1
fi
