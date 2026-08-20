#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
header=$root/include/pkgctl/run_resource.h
source=$root/src/run_resource.cpp
locator_header=$root/include/pkgctl/run_locator.h
locator_source=$root/src/run_locator.cpp
construction=$root/src/construction.cpp
runtime=$root/src/run_runtime.cpp
command=$root/cli/run_command.cpp
options=$root/cli/options.cpp
pipeline=$root/tests/integration/package_pipeline_test.cpp
locator_test=$root/tests/unit/run_locator_test.cpp
runtime_test=$root/tests/unit/construction_test.cpp
readonly=$root/tests/integration/cli_readonly_test.sh

fail()
{
  echo "run-resource-contract: $*" >&2
  exit 1
}

require()
{
  file=$1
  text=$2
  grep -F -- "$text" "$file" >/dev/null || \
    fail "${file#$root/} omits: $text"
}

forbid()
{
  file=$1
  text=$2
  if grep -F -- "$text" "$file" >/dev/null 2>&1; then
    fail "${file#$root/} retains forbidden authority: $text"
  fi
}

for file in "$header" "$source" "$locator_header" "$locator_source" \
            "$construction" "$runtime" "$command" "$options" \
            "$pipeline" "$locator_test" "$runtime_test" "$readonly"; do
  [ -s "$file" ] || fail "missing resource-plane source: ${file#$root/}"
done

# Effectful resource preparation consumes only already-sealed installed package
# provenance plus the present exact-byte provider. Image semantics remain with
# libpkgimage-exec, and the pure locator receives only an in-memory mapping.
for text in \
  'class native_transaction_resource_session_source final' \
  'pkgobject::store* package_objects' \
  'native package-object authority is unavailable for installed package resource preparation' \
  'native package-object authority is unavailable for fresh construction' \
  'if (installed_inputs.empty())' \
  'require_construction_package_object_authority(package_objects_)' \
  'build.artifact_content().string()' \
  'build.artifact_image().string()' \
  'auto& objects = require_package_object_authority(package_objects);' \
  'objects.require(content)' \
  'pkgimage_exec::realize_package_tree' \
  'pkgctl/native-installed-package-resource/1' \
  'configuration.roots().installed_resource_root' \
  'construction_installed_inputs' \
  'check_installed_inputs' \
  'native_transaction_dispatch_session_source locator' \
  'prepared_installed_package_source prepared'; do
  grep -F -- "$text" "$header" "$source" >/dev/null || \
    fail "resource adapter omits: $text"
done

# Provider API ownership is explicit in the effectful implementation. The
# controller header needs only the borrowed store type and must not transitively
# expose the provider implementation to every pkgctl include consumer.
forbid "$header" '<libpkgobject/'
require "$source" '#include <libpkgobject/libpkgobject.h>'

# Resource failure is not a policy-feedback edge. This source may not consult
# resolution/catalog/history/target discovery or enumerate ambient files.
for token in \
  'pkgresolve::' \
  'libpkgresolve' \
  'pkgcatalog::' \
  'libpkgcatalog' \
  'transaction_history' \
  'target_root' \
  'directory_iterator' \
  'recursive_directory_iterator' \
  'weakly_canonical(' \
  'canonical('; do
  forbid "$source" "$token"
done

# The existing locator remains observation-free; the new provider is never
# imported into that pure authority-to-location boundary.
for token in \
  'libpkgobject' \
  'pkgobject::' \
  'libpkgimage-exec' \
  'pkgimage_exec::'; do
  forbid "$locator_header" "$token"
  forbid "$locator_source" "$token"
done

# Exact construction publication is the population edge. Public projection
# precedes reservoir admission so replay can repeat the same operation from
# retained terminal construction authority.
require "$construction" 'pkgbuild_exec::publish_sealed_artifact(session, result);'
require "$construction" 'package_objects_->admit('
require "$construction" 'require_package_object_publication_'
require "$construction" 'native construction package-object authority is unavailable'
publish_line=$(grep -n -F 'pkgbuild_exec::publish_sealed_artifact(session, result);' \
  "$construction" | head -n 1 | cut -d: -f1)
admit_line=$(grep -n -F 'package_objects_->admit(' "$construction" | head -n 1 | cut -d: -f1)
[ "$publish_line" -lt "$admit_line" ] || \
  fail 'package-object admission precedes exact public artifact projection'

# Native runtime owns the effectful adapter and passes the same store to
# construction publication. The generic run kernel stays unaware of provider
# lookup semantics.
require "$runtime" 'sessions_(configuration_.sessions(), backends.package_objects)'
require "$runtime" 'sessions_, backends.package_objects, true, operations_.get()'
require "$runtime" 'native_transaction_resource_session_source sessions_;'

# CLI authority is the provider namespace, not caller-authored installed trees.
require "$options" '--package-object-store PATH'
require "$command" 'pkgobject::store::open_or_create('
require "$command" 'require_package_object_store_separation(command);'
forbid "$options" '--installed-tree'
forbid "$command" 'installed_tree'


# Least authority is behavioral: CHECK with no installed input delegates without
# a package-object provider, while already-terminal runtime recovery remains
# quiescent with neither the provider nor process backends.
require "$locator_test" 'native_transaction_resource_session_source resource_sessions('
require "$locator_test" 'configuration(root / "resource-runtime"), nullptr'
require "$locator_test" 'resource_only_check = resource_sessions.check('
require "$runtime_test" 'terminal_without_package_objects'
require "$runtime_test" '{nullptr, nullptr, nullptr, archive_backend}'

# The vertical proves real installed BUILD/CHECK use and hostile present-resource
# loss while old construction residue remains available but unused.
for text in \
  'installed_consumer_resolution_request' \
  'retained_installed_tool' \
  'consumer build did not receive installed tool resource' \
  'consumer check did not receive installed tool resource' \
  'package_objects.require(installed_tool_content)' \
  'pkgobject::error_code::object_unavailable' \
  'pkgobject::error_code::corrupt_object' \
  'completed_tool_construction->session().paths().build.artifact_path' \
  'backend.build_calls() == build_calls_before_missing' \
  'backend.check_calls() == check_calls_before_corrupt'; do
  require "$pipeline" "$text"
done

# Process-level CLI qualification refuses namespace overlap before creating the
# provider root and no longer advertises the retired escape hatch.
require "$readonly" "require_help_text '--package-object-store PATH'"
require "$readonly" "require_help_absent '--installed-tree'"
require "$readonly" 'package-object store must be disjoint from private runtime root'
require "$readonly" '[ ! -e "$overlap_runtime/package-objects" ]'
