#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
test="$srcdir/tests/integration/cli_run_lease_loss_test.sh"
fixture="$srcdir/tests/fixtures/native_target_lock_revoker.cpp"
credential_runner="$srcdir/tests/fixtures/native_credential_context_runner.cpp"
credential_preload="$srcdir/tests/fixtures/native_credential_context_preload.cpp"
interpreter="$srcdir/tests/fixtures/native_lease_loss_interpreter_x86_64.S"
recipe="$srcdir/tests/fixtures/collections/lifecycle-post-install-lease-loss/fixture/recipe.yml"
meson="$srcdir/tests/meson.build"
direct="$srcdir/tests/run-direct.sh"

for path in "$test" "$fixture" "$credential_runner" "$credential_preload" "$interpreter" "$recipe" "$meson" "$direct"; do
  [ -s "$path" ] || {
    echo "missing CLI lease-loss qualification source: $path" >&2
    exit 1
  }
done

for required in \
  "lifecycle_goal='lifecycle:post-install=fixture'" \
  '"$revoker" "$runtime/target-locks" "$target"' \
  'disposition external-resolution-required' \
  'dispatch.$operation_index.state=started' \
  'dispatch.$operation_index.observations=1' \
  'run.external-evidence-required=true' \
  'effect.stage=terminal' \
  'effect.disposition=terminal' \
  'effect.automatically-continuable=true' \
  'effect.external-resolution-required=false' \
  'effect.application-outcome=completed' \
  'effect.terminal-outcome=outer-lease-lost' \
  'effect.publication-request=' \
  'packages 0' \
  'post-install-ran' \
  'rm -rf "$collection"' \
  'supervisor_uid=65534' \
  'chown -R 65534:65534 "$root"' \
  "'steps 1'" \
  "'durable-steps 0'" \
  'resumed-run-record' \
  'resumed-effect-record' \
  'repeated-run-record' \
  'repeated-effect-record' \
  'recreated $lock_count target locks, expected 0'; do
  grep -F -- "$required" "$test" >/dev/null || {
    echo "CLI lease-loss test omits boundary assertion: $required" >&2
    exit 1
  }
done

for required in \
  'mkfifoat' \
  'AT_SYMLINK_NOFOLLOW' \
  'target-locks authority' \
  'unlinkat(lock_fd.get(), entries.front().c_str(), 0)' \
  'target-lock directory does not contain exactly one anchored lease' \
  'write_all(acknowledge_fd.get(), "revoked\n")'; do
  grep -F -- "$required" "$fixture" >/dev/null || {
    echo "native lock revoker omits physical lease-loss mechanism: $required" >&2
    exit 1
  }
done

for required in \
  "cmpb \$'#'" \
  '.asciz "/target/post-install-ran"' \
  '.asciz "/target/.pkgctl-test-lease-loss-ready"' \
  '.asciz "/target/.pkgctl-test-lease-loss-ack"'; do
  grep -F -- "$required" "$interpreter" >/dev/null || {
    echo "native lease-loss interpreter omits lifecycle synchronization: $required" >&2
    exit 1
  }
done

for required in \
  '.pkgctl-test-lease-loss-ready' \
  '.pkgctl-test-lease-loss-ack' \
  '# pkgctl-test-lease-loss-lifecycle' \
  'post-install ran' \
  '[ "$acknowledgement" = revoked ]'; do
  grep -F -- "$required" "$recipe" >/dev/null || {
    echo "lease-loss lifecycle fixture omits synchronization contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  'effect.disposition=external-resolution-required' \
  'effect.automatically-continuable=false' \
  'effect.external-resolution-required=true' \
  'flock ' \
  'target_mutation_lease::acquire' \
  'target_mutation_lease_error' \
  'kill -KILL' \
  'sleep 1'; do
  if grep -F -- "$forbidden" "$test" "$fixture" >/dev/null; then
    echo "CLI lease-loss qualification owns forbidden mechanism/policy: $forbidden" >&2
    exit 1
  fi
done

grep -F "'cli-run-lease-loss'" "$meson" >/dev/null || {
  echo 'Meson omits privileged CLI lease-loss vertical' >&2
  exit 1
}
grep -F 'native_lease_loss_interpreter' "$meson" >/dev/null || {
  echo 'Meson omits dedicated native lease-loss interpreter' >&2
  exit 1
}
grep -F 'native_target_lock_revoker' "$meson" >/dev/null || {
  echo 'Meson omits native target lock-revoker dependency' >&2
  exit 1
}
grep -F 'native_credential_context_runner' "$meson" >/dev/null || {
  echo 'Meson omits native credential-context dependency' >&2
  exit 1
}
grep -F 'native_credential_context_preload' "$meson" >/dev/null || {
  echo 'Meson omits post-loader credential-context preload' >&2
  exit 1
}
grep -F "'cli-lease-loss-contract'" "$meson" >/dev/null || {
  echo 'Meson omits CLI lease-loss contract' >&2
  exit 1
}
grep -F 'cli_run_lease_loss_test.sh' "$direct" >/dev/null || {
  echo 'direct compiler path omits CLI lease-loss vertical' >&2
  exit 1
}
grep -F 'native_lease_loss_interpreter_x86_64.S' "$direct" >/dev/null || {
  echo 'direct compiler path omits native lease-loss interpreter' >&2
  exit 1
}
grep -F 'native_target_lock_revoker.cpp' "$direct" >/dev/null || {
  echo 'direct compiler path omits native target lock revoker' >&2
  exit 1
}
grep -F 'native_credential_context_runner.cpp' "$direct" >/dev/null || {
  echo 'direct compiler path omits native credential-context runner' >&2
  exit 1
}
grep -F 'native_credential_context_preload.cpp' "$direct" >/dev/null || {
  echo 'direct compiler path omits post-loader credential-context preload' >&2
  exit 1
}

for required in \
  'restore_loader_environment' \
  '::setgroups(0, nullptr)' \
  '::setgid(gid)' \
  '::setuid(uid)'; do
  grep -F -- "$required" "$credential_preload" >/dev/null || {
    echo "credential-context preload omits post-loader transition: $required" >&2
    exit 1
  }
done
if grep -F '::setuid(' "$credential_runner" >/dev/null; then
  echo 'credential-context runner changes credentials before loading pkgctl' >&2
  exit 1
fi
