#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
test_file=$root/tests/integration/cli_run_shared_ownership_restart_test.sh
collection=$root/tests/fixtures/collections/shared-ownership-restart
runtime_recipe=$collection/runtime-lib/recipe.yml
meson=$root/tests/meson.build

for path in "$test_file" "$runtime_recipe" "$collection/base-files/recipe.yml"; do
  [ -s "$path" ] || {
    echo "missing shared-ownership restart qualification source: $path" >&2
    exit 1
  }
done

# The second owner is explicitly ordered after the first by source authority;
# the restart test must not infer package order from filenames or dispatch ids.
grep -F -- 'requirements:' "$runtime_recipe" >/dev/null
grep -F -- '  run:' "$runtime_recipe" >/dev/null
grep -F -- '    - package: base-files' "$runtime_recipe" >/dev/null

for required in \
  "--goal 'run=runtime-lib'" \
  '--operation-policy exact-compatible-sharing' \
  'publication-terminal' \
  'effect.stage=publication-terminal' \
  'packages 1' \
  'owner.0 base-files ' \
  '--operation-policy strict-exclusive' \
  '--resume uses retained transaction semantics' \
  'origin resumed' \
  'origin retained-existing' \
  'owners 2' \
  'owner.1 runtime-lib ' \
  'retained-shared-path-not-rewritten' \
  'effect.stage=terminal' \
  'findings 0' \
  'failures 0'; do
  grep -F -- "$required" "$test_file" >/dev/null || {
    echo "shared-ownership restart qualification omits: $required" >&2
    exit 1
  }
done

# The valid resume block must consume retained authority and must not redeclare
# catalog, goals, build policy, or operation policy from current CLI state.
resume_block=$(sed -n '/^run_resume()$/,/^}$/p' "$test_file")
for forbidden in \
  '--collection' \
  '--goal' \
  '--build-parallelism' \
  '--build-source-date-epoch' \
  '--operation-policy'; do
  if printf '%s\n' "$resume_block" | grep -F -- "$forbidden" >/dev/null 2>&1; then
    echo "valid shared-ownership resume redeclares start authority: $forbidden" >&2
    exit 1
  fi
done
for required in \
  '--canonical-store "$state"' \
  '--resume "$run_nonce"' \
  '--runtime-root "$runtime"' \
  '--target-root "$target"'; do
  printf '%s\n' "$resume_block" | grep -F -- "$required" >/dev/null || {
    echo "valid shared-ownership resume omits live resume authority: $required" >&2
    exit 1
  }
done

# The test may observe public owner APIs and target bytes, but must not decode
# private controller/adapter bodies to reconstruct policy or ownership meaning.
for forbidden in \
  'operation-session-' \
  'publication-request-' \
  'publication-receipt-' \
  'completed-evidence-' \
  'effect-body-' \
  'tar -' \
  'tar -t' \
  'tar -x'; do
  if grep -F -- "$forbidden" "$test_file" >/dev/null 2>&1; then
    echo "shared-ownership restart test reconstructs foreign/private meaning: $forbidden" >&2
    exit 1
  fi
done

for required in \
  "'cli-run-shared-ownership-restart'" \
  'cli_run_shared_ownership_restart_test' \
  "'tests/fixtures/collections/shared-ownership-restart'" \
  'publication_terminal_interrupt_fixture' \
  'state_ownership_inspect_fixture' \
  'rootfs_audit_fixture'; do
  grep -F -- "$required" "$meson" >/dev/null || {
    echo "Meson omits shared-ownership restart wiring: $required" >&2
    exit 1
  }
done
