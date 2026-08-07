#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/preparation.h"
source="$srcdir/src/preparation.cpp"

for file in "$header" "$source"; do
  [ -s "$file" ] || {
    echo "missing operation preparation authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class operation_preparation_request final' \
  'class operation_preparation_driver' \
  'class native_operation_preparation_driver final' \
  'class operation_planning_refusal final' \
  'class operation_preparation_result final' \
  'pkgstate::plan_adapter::project_installed_state' \
  'pkgbuild::plan_adapter::project_artifact' \
  'pkgapply::incoming_package_authority::admit' \
  'pkgplan::plan_install' \
  'pkgplan::plan_upgrade' \
  'pkgplan::plan_removal' \
  'pkgapply::installation_application_request::make' \
  'pkgapply::upgrade_application_request::make' \
  'pkgapply::removal_application_request::make' \
  'effectful_operation_request::make' \
  'planning_refused' \
  'preparation_driver_contract_violation'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing operation preparation contract: $required" >&2
    exit 1
  }
done

state_line=$(grep -n 'auto installed = project_state' "$source" | head -n1 | cut -d: -f1)
artifact_line=$(grep -n 'driver.project_artifact' "$source" | head -n1 | cut -d: -f1)
plan_line=$(grep -n 'pkgplan::plan_install' "$source" | head -n1 | cut -d: -f1)
effect_line=$(grep -n 'effectful_operation_request::make' "$source" | tail -n1 | cut -d: -f1)
[ -n "$state_line" ] && [ -n "$artifact_line" ] && \
  [ -n "$plan_line" ] && [ -n "$effect_line" ] && \
  [ "$state_line" -lt "$artifact_line" ] && \
  [ "$artifact_line" -lt "$plan_line" ] && \
  [ "$plan_line" -lt "$effect_line" ] || {
  echo 'incoming operation preparation authority order is not explicit' >&2
  exit 1
}

for forbidden in \
  'pkgapply::apply' \
  'pkgapply_exec::execute' \
  'pkgexec::execute' \
  'compare_and_publish' \
  'canonical_generation_store' \
  'libpkgexec-linux' \
  'pkgmk' \
  'pkgman'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden operation preparation shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E 'prepare_operation|operation_preparation_request' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'operation preparation must not acquire a command frontend' >&2
  exit 1
fi


for document in \
  "$srcdir/README.md" \
  "$srcdir/DESIGN.md" \
  "$srcdir/MAINTAINING.md" \
  "$srcdir/TESTING.md"; do
  [ -s "$document" ] || {
    echo "missing operation preparation documentation: $document" >&2
    exit 1
  }
done

grep -F 'retained build/image admission' \
  "$srcdir/README.md" >/dev/null
grep -F 'must not reopen artifact bytes' \
  "$srcdir/MAINTAINING.md" >/dev/null
grep -F \
  'retained build/image admission projected without reopening artifact bytes' \
  "$srcdir/TESTING.md" >/dev/null
grep -F 'removed controller artifact I/O' \
  "$srcdir/DESIGN.md" >/dev/null

for obsolete in \
  'reinspection retains the construction archive digest' \
  'artifact reinspection reproducing construction archive' \
  'It may inspect exact artifact bytes through an injected backend' \
  'reinspect and project incoming artifact through libpkgbuild-plan' \
  'every exact `libpkgbuild` package-input subject and tree identity' \
  'translate observed digests into the complete libpkgbuild source set'; do
  if grep -F "$obsolete" \
      "$srcdir/README.md" \
      "$srcdir/DESIGN.md" \
      "$srcdir/MAINTAINING.md" \
      "$srcdir/TESTING.md" >/dev/null 2>&1; then
    echo "obsolete preparation authority remains in documentation: $obsolete" >&2
    exit 1
  fi
done
