#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
test_source=$srcdir/tests/integration/package_pipeline_test.cpp

[ -s "$test_source" ] || {
  echo 'missing package-pipeline integration source' >&2
  exit 1
}

for authority in \
  'pkgctl::acquire_catalog' \
  'pkgctl::resolve_packages' \
  'pkgctl::compose_transaction' \
  'pkgctl::transaction_run::begin' \
  'pkgctl::reserve_next' \
  'pkgctl::native_transaction_dispatch_session_source' \
  'pkgctl::native_construction_driver' \
  'pkgctl::execute_construction' \
  'pkgctl::complete_construction_dispatch' \
  'pkgctl::prepare_operation'; do
  grep -F "$authority" "$test_source" >/dev/null || {
    echo "package-pipeline test bypasses production authority: $authority" >&2
    exit 1
  }
done

grep -F 'public pkgexec::execution_backend' "$test_source" >/dev/null || {
  echo 'package-pipeline test does not isolate process execution behind backend injection' >&2
  exit 1
}
grep -F 'dependency.session().paths().build.package_output_root' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline test does not prove predecessor package-tree reuse' >&2
  exit 1
}
grep -F 'pkgexec::resource_role::build_input_tree, "dep"' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline backend does not consume the dependency input slot' >&2
  exit 1
}

for forbidden in 'pkgctl_exe' 'std::system(' '::execv(' '::execve('; do
  if grep -F "$forbidden" "$test_source" >/dev/null; then
    echo "package-pipeline test escaped into command/process orchestration: $forbidden" >&2
    exit 1
  fi
done
