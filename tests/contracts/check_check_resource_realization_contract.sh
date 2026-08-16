#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
locator="$root/src/run_locator.cpp"
check="$root/src/check.cpp"
archive_test="$root/tests/integration/cli_build_archive_source_test.sh"
probe="$root/tests/fixtures/collections/archive-source-check/archive-probe/recipe.yml"
dep="$root/tests/fixtures/collections/archive-source-check/archive-dep/recipe.yml"

fail()
{
  echo "check-resource-realization-contract: $*" >&2
  exit 1
}

require()
{
  file=$1
  text=$2
  grep -F -- "$text" "$file" >/dev/null ||
    fail "${file#$root/} omits: $text"
}

forbid()
{
  file=$1
  text=$2
  if grep -F -- "$text" "$file" >/dev/null; then
    fail "${file#$root/} contains forbidden check-resource coupling: $text"
  fi
}

require "$locator" 'source_object_resource_identity('
require "$locator" 'checked_package_resource_identity('
require "$locator" 'constructed_input_resource_identity('
require "$locator" 'roots.check_resource_root / scope / "source"'
require "$locator" 'roots.check_resource_root / scope / "package"'
require "$locator" 'roots.check_resource_root / scope / "inputs" /'
require "$locator" 'request.constructed_inputs()'
require "$locator" 'retained_installed_input_resource('
forbid "$locator" 'require_construction_input_resource('
forbid "$locator" 'pkgbuild_exec::project_prepared_paths'
forbid "$locator" 'prepared_paths.source_tree'

check_block=$(sed -n \
  '/native_transaction_dispatch_session_source::check(/,/^}/p' "$locator")
printf '%s\n' "$check_block" | grep -F 'check_input_resources(request, roots, scope, installed_packages_)' >/dev/null ||
  fail 'native check locator does not allocate independent check inputs'
printf '%s\n' "$check_block" | grep -F 'package_output_root' >/dev/null &&
  fail 'native check locator borrows construction package-output residue'

require "$check" 'pkgsource_exec::realize_source_object_tree('
require "$check" 'construction.materialization()'
require "$check" 'realize_construction_package('
require "$check" 'session.request().constructed_inputs()'
require "$check" 'image_authority->image().receipt().archive_digest()'
require "$check" 'construction.session().paths().build.artifact_path'

require "$probe" 'package: archive-dep'
require "$dep" 'archive-dependency'
require "$archive_test" '--max-steps 2'
require "$archive_test" 'rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"'
require "$archive_test" 'check resume after construction-residue removal failed'
require "$archive_test" 'terminal cleanup retained private realization under $directory'

# Execution resource identities are controller-owned per-instance bindings.
grep -F 'pkgctl/native-check-source-resource/1' "$locator" >/dev/null || { echo 'check-resource contract: source instance identity is not controller-owned' >&2; exit 1; }
grep -F 'pkgctl/native-check-package-resource/1' "$locator" >/dev/null || { echo 'check-resource contract: checked-package instance identity is not controller-owned' >&2; exit 1; }
grep -F 'pkgctl/native-check-input-resource/1' "$locator" >/dev/null || { echo 'check-resource contract: constructed-input instance identity is not controller-owned' >&2; exit 1; }
! grep -F 'pkgsource_exec::source_object_tree_identity' "$locator" "$check" >/dev/null || { echo 'check-resource contract: source realizer leaked execution-resource identity ownership' >&2; exit 1; }
! grep -F 'pkgimage_exec::package_tree_identity' "$locator" "$check" >/dev/null || { echo 'check-resource contract: image realizer leaked execution-resource identity ownership' >&2; exit 1; }
