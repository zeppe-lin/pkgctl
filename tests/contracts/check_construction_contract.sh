#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/construction.h"
source="$srcdir/src/construction.cpp"
codec="$srcdir/include/pkgctl/construction_codec.h"
codec_source="$srcdir/src/construction_session_codec.cpp"

for file in "$header" "$source" "$codec" "$codec_source"; do
  [ -s "$file" ] || {
    echo "missing construction authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class construction_request final' \
  'class construction_session final' \
  'construction_session_encoding_version = 1' \
  'construction_codec_error_code' \
  'construction_codec_error final' \
  'encode_construction_session' \
  'decode_construction_session' \
  'class construction_driver' \
  'class native_construction_driver final' \
  'pkgfetch::materialize' \
  'pkgbuild::build_request::seal' \
  'validate_input_resources' \
  'transaction.resolution().resolution()' \
  'pkgbuild_exec::admitted_build_session::admit' \
  'pkgbuild_exec::execute_sealed' \
  'pkgbuild_exec::publish_sealed_artifact' \
  'execute_construction_unpublished' \
  'publish_construction' \
  'construction_driver_contract_violation' \
  'image_authority'; do
  grep -F -- "$required" "$header" "$source" "$codec" \
      "$codec_source" >/dev/null || {
    echo "missing construction authority contract: $required" >&2
    exit 1
  }
done


# Concrete construction resources are build-scope only.
grep -F 'request.build().inputs().for_scope(pkgbuild::input_scope::build)' \
    "$source" >/dev/null || {
  echo 'construction admission does not isolate build-scoped inputs' >&2
  exit 1
}
if grep -F 'PKG_CHECK_INPUT' "$source" >/dev/null 2>&1; then
  echo 'construction source leaked check execution resources' >&2
  exit 1
fi

materialize_line=$(grep -n 'driver.materialize_source' "$source" | head -n1 | cut -d: -f1)
build_line=$(grep -n 'driver.execute_build' "$source" | head -n1 | cut -d: -f1)
[ -n "$materialize_line" ] && [ -n "$build_line" ] && \
    [ "$materialize_line" -lt "$build_line" ] || {
  echo 'construction does not materialize before build execution' >&2
  exit 1
}

publish_line=$(grep -n 'driver.publish_build' "$source" | head -n1 | cut -d: -f1)
[ -n "$build_line" ] && [ -n "$publish_line" ] && \
    [ "$build_line" -lt "$publish_line" ] || {
  echo 'construction publication does not follow build sealing' >&2
  exit 1
}

for forbidden in \
  'libpkgexec-linux' \
  'canonical_generation_store::initialize' \
  'driver.publish_state' \
  'pkgapply::apply' \
  'pkgmk' \
  'pkgman'; do
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden construction authority shortcut: $forbidden" >&2
    exit 1
  fi
done


for forbidden in \
  'std::filesystem::exists(' \
  'std::filesystem::status(' \
  'std::ifstream' \
  'std::ofstream' \
  'opendir(' \
  'readdir(' \
  'glob('; do
  if grep -F -- "$forbidden" "$codec_source" >/dev/null 2>&1; then
    echo "construction codec must not observe host authority: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E 'execute_construction|native_construction_driver' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'construction command frontend must remain read-only' >&2
  exit 1
fi
