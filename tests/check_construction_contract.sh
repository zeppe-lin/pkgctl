#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/construction.h"
source="$srcdir/src/construction.cpp"

for file in "$header" "$source"; do
  [ -s "$file" ] || {
    echo "missing construction authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class construction_request final' \
  'class construction_session final' \
  'class construction_driver' \
  'class native_construction_driver final' \
  'pkgfetch::materialize' \
  'pkgbuild::build_request::seal' \
  'validate_package_input_authority' \
  'transaction.resolution().resolution()' \
  'pkgbuild_exec::admitted_build_session::admit' \
  'pkgbuild_exec::execute' \
  'construction_driver_contract_violation' \
  'artifact_inspection'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing construction authority contract: $required" >&2
    exit 1
  }
done

materialize_line=$(grep -n 'driver.materialize_source' "$source" | head -n1 | cut -d: -f1)
build_line=$(grep -n 'driver.execute_build' "$source" | head -n1 | cut -d: -f1)
[ -n "$materialize_line" ] && [ -n "$build_line" ] && \
    [ "$materialize_line" -lt "$build_line" ] || {
  echo 'construction does not materialize before build execution' >&2
  exit 1
}

for forbidden in \
  'libpkgexec-linux' \
  'canonical_generation_store::initialize' \
  'driver.publish_state' \
  'pkgapply::apply' \
  'pkgmk' \
  'pkgman'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden construction authority shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E 'execute_construction|native_construction_driver' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'construction command frontend must remain read-only' >&2
  exit 1
fi
