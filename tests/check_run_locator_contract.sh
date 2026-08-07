#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
header=$root/include/pkgctl/run_locator.h
source=$root/src/run_locator.cpp
check_source=$root/src/check.cpp
test_source=$root/tests/run_locator_test.cpp

for file in "$header" "$source" "$test_source"; do
  [ -f "$file" ] || { echo "missing native locator file: $file" >&2; exit 1; }
done

grep -q 'class retained_installed_package_tree_source' "$header"
grep -q 'class native_transaction_dispatch_session_source' "$header"
grep -q 'native_transaction_session_configuration' "$header"
grep -q 'record.journal().hex()' "$source"
grep -q 'dispatch.identity().hex()' "$source"
grep -q 'project_prepared_paths' "$source"
grep -q 'require_predecessor_construction' "$source"
grep -q 'installed_packages.locate' "$source"
grep -q 'pkgcheck_exec::seal_execution_request' "$check_source"
grep -q 'check_installed_input_location' "$test_source"
grep -q 'started_record' "$test_source"
grep -q 'started_check_record' "$test_source"

# Location translates exact authority to call-scoped paths and nothing more.
if grep -E -n   'create_directories|remove_all|directory_iterator|recursive_directory_iterator|canonical\(|weakly_canonical\(|exists\(|status\(|symlink_status\(|chmod\(|chown\(|stat\(|lstat\(|open\(|pkgfetch::materialize|execute_construction|execute_check|advance_(construction|check)|journal_store|evidence_store'   "$header" "$source"; then
  echo 'native locator performs or imports forbidden host/control effects' >&2
  exit 1
fi

# The locator remains controller-private and the CLI remains read-only.
! grep -R -q 'run_locator' "$root/cli"
! grep -E -q '(^|[[:space:]])(run|apply|install|upgrade|remove)([[:space:]]|$)'   "$root/cli/main.cpp"

grep -q 'Release 0.31.0 supplies the first native implementation'   "$root/README.md"
grep -q 'Release 0.31.0 native session/resource locator boundary'   "$root/DESIGN.md"
grep -q 'Release 0.31.0 native locator qualification' "$root/TESTING.md"
grep -q 'NATIVE CONSTRUCTION AND CHECK SESSION LOCATION'   "$root/man/pkgctl_orchestration.7.scd"
