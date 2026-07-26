#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

for compiler in g++ clang++; do
  command -v "$compiler" >/dev/null 2>&1 || {
    echo "required compiler not found: $compiler" >&2
    exit 1
  }
  CXX="$compiler" "$srcdir/tests/run-direct.sh"
done

for script in "$srcdir"/ci/*.sh "$srcdir"/tests/*.sh; do
  sh -n "$script"
done

"$srcdir/tests/check_release_metadata.sh" "$srcdir"

git -C "$srcdir" diff --check
git -C "$srcdir" show --check --oneline HEAD >/dev/null
git -C "$srcdir" fsck --no-dangling >/dev/null
