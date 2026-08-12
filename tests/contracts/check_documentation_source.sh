#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}

for document in CHANGELOG.md CONTRIBUTING.md DESIGN.md MAINTAINING.md README.md TESTING.md; do
  file="$srcdir/$document"
  first=$(sed -n '1p' "$file")
  case $first in
    '# '*) ;;
    *)
      echo "root documentation lacks an ATX level-one title: $document" >&2
      exit 1
      ;;
  esac

  count=$(grep -c '^# ' "$file" || :)
  [ "$count" -eq 1 ] || {
    echo "root documentation has $count level-one titles: $document" >&2
    exit 1
  }

  if grep -n -E '^(=+|-+|~+)$' "$file" >/dev/null; then
    echo "root documentation contains Setext heading syntax: $document" >&2
    exit 1
  fi
done

echo 'documentation-source-contract: ok'
