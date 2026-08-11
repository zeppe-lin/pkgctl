#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
main=$srcdir/cli/main.cpp
root_meson=$srcdir/meson.build
core_meson=$srcdir/src/meson.build
cli_meson=$srcdir/cli/meson.build

fail()
{
  echo "cli-yaml-boundary: $*" >&2
  exit 1
}

grep -F '#include <libpkgsource-yaml/parser.h>' "$main" >/dev/null ||
  fail 'CLI does not include the YAML adapter diagnostic type'
grep -F 'catch (const pkgsource::yaml::yaml_error& value)' "$main" >/dev/null ||
  fail 'CLI does not catch the current YAML adapter exception'

if grep -R -n -F 'yaml_adapter' \
    "$srcdir/include" "$srcdir/src" "$srcdir/cli" >/dev/null 2>&1; then
  grep -R -n -F 'yaml_adapter' \
    "$srcdir/include" "$srcdir/src" "$srcdir/cli" >&2 || true
  fail 'retired YAML adapter namespace remains in controller source'
fi

if grep -R -n -E 'libpkgsource-yaml|pkgsource::yaml' \
    "$srcdir/include" "$srcdir/src" >/dev/null 2>&1; then
  grep -R -n -E 'libpkgsource-yaml|pkgsource::yaml' \
    "$srcdir/include" "$srcdir/src" >&2 || true
  fail 'YAML syntax authority leaked into the controller core'
fi

grep -F "libpkgsource_yaml_dep = dependency(" "$root_meson" >/dev/null ||
  fail 'root build does not resolve the YAML adapter explicitly'

core_block=$(sed -n \
  '/^pkgctl_core_authority_deps = \[/,/^]/p' "$root_meson")
printf '%s\n' "$core_block" | grep -F 'libpkgsource_yaml_dep' >/dev/null &&
  fail 'YAML adapter leaked into pkgctl_core_authority_deps'

grep -F 'dependencies: pkgctl_core_authority_deps,' "$core_meson" >/dev/null ||
  fail 'controller core does not use its reviewed dependency closure'
cli_block=$(sed -n '/^  dependencies: \[/,/^  \],/p' "$cli_meson")
printf '%s\n' "$cli_block" | grep -F 'libpkgsource_yaml_dep' >/dev/null ||
  fail 'CLI does not declare the YAML adapter as a direct dependency'
for dependency in libpkgcatalog_codec_dep libpkgexec_linux_dep; do
  printf '%s\n' "$cli_block" | grep -F -- "$dependency" >/dev/null ||
    fail "CLI does not declare its native run dependency: $dependency"
  if printf '%s\n' "$core_block" | grep -F -- "$dependency" >/dev/null; then
    fail "CLI-only native run dependency leaked into controller core: $dependency"
  fi
done
