#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cxx=${CXX:-c++}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-direct.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

core_modules='libcrypto libpkgsource libpkgcatalog libpkgcatalog-acquire libpkgstate libpkgstate-posix libpkgstate-plan libpkgstate-apply libpkgfetch libpkgsource-exec libpkgbuild libpkgbuild-exec libpkgbuild-image libpkgsource-plan libpkgbuild-plan libpkgimage libpkgimage-exec libpkgplan libpkgexec libpkgapply libpkgapply-posix libpkgapply-exec libpkgresolve libpkgtransaction libpkgcheck libpkgcheck-exec'
cli_modules='libpkgsource-yaml libpkgcatalog-codec libpkgexec-linux'
pipeline_modules='libpkgreconcile libpkgreconcile-apply libpkgreconcile-posix libpkgreconcile-apply-posix'
audit_modules='libpkgaudit'
pkg-config --exists \
  'libpkgsource >= 4.1.0' 'libpkgsource < 5.0.0' \
  'libpkgsource-yaml >= 2.0.0' 'libpkgsource-yaml < 3.0.0' \
  'libpkgsource-plan >= 2.0.0' 'libpkgsource-plan < 3.0.0' \
  'libpkgcatalog >= 4.0.0' 'libpkgcatalog < 5.0.0' \
  'libpkgcatalog-codec >= 4.0.0' 'libpkgcatalog-codec < 5.0.0' \
  'libpkgcatalog-acquire >= 4.0.0' 'libpkgcatalog-acquire < 5.0.0' \
  'libpkgfetch >= 3.0.0' 'libpkgfetch < 4.0.0' \
  'libpkgsource-exec >= 0.1.0' 'libpkgsource-exec < 1.0.0' \
  'libpkgbuild >= 3.0.1' 'libpkgbuild < 4.0.0' \
  'libpkgbuild-exec >= 3.2.0' 'libpkgbuild-exec < 4.0.0' \
  'libpkgbuild-image >= 1.0.1' 'libpkgbuild-image < 2.0.0' \
  'libpkgimage-exec >= 0.1.0' 'libpkgimage-exec < 1.0.0' \
  'libpkgbuild-plan >= 1.1.0' 'libpkgbuild-plan < 2.0.0' \
  'libpkgstate-apply >= 3.1.1' 'libpkgstate-apply < 4.0.0' \
  'libpkgexec >= 2.1.1' 'libpkgexec < 3.0.0' \
  'libpkgexec-linux >= 0.6.2' 'libpkgexec-linux < 1.0.0' \
  'libpkgapply >= 3.0.1' 'libpkgapply < 4.0.0' \
  'libpkgapply-posix >= 3.2.1' 'libpkgapply-posix < 4.0.0' \
  'libpkgapply-exec >= 3.0.1' 'libpkgapply-exec < 4.0.0' \
  'libpkgresolve >= 4.0.0' 'libpkgresolve < 5.0.0' \
  'libpkgtransaction >= 4.0.0' 'libpkgtransaction < 5.0.0' \
  'libpkgcheck >= 0.3.0' 'libpkgcheck < 1.0.0' \
  'libpkgcheck-exec >= 0.6.0' 'libpkgcheck-exec < 1.0.0' \
  'libpkgreconcile >= 0.3.0' 'libpkgreconcile < 1.0.0' \
  'libpkgreconcile-apply >= 0.1.1' 'libpkgreconcile-apply < 1.0.0' \
  'libpkgreconcile-posix >= 0.1.0' 'libpkgreconcile-posix < 1.0.0' \
  'libpkgreconcile-apply-posix >= 0.1.1' 'libpkgreconcile-apply-posix < 1.0.0' \
  'libpkgaudit >= 0.1.0' 'libpkgaudit < 1.0.0'
core_cflags=$(pkg-config --cflags $core_modules)
core_libs=$(pkg-config --libs $core_modules)
cli_cflags=$(pkg-config --cflags $cli_modules)
cli_libs=$(pkg-config --libs $cli_modules)
pipeline_cflags=$(pkg-config --cflags $pipeline_modules)
pipeline_libs=$(pkg-config --libs $pipeline_modules)
audit_cflags=$(pkg-config --cflags $audit_modules)
audit_libs=$(pkg-config --libs $audit_modules)
flags="-std=c++17 -Wall -Wextra -Wpedantic -Werror -I$srcdir/include -I$srcdir/tests $core_cflags"
objects=
for source in "$srcdir"/src/*.cpp; do
  object="$tmp/$(basename "$source").o"
  # shellcheck disable=SC2086
  "$cxx" $flags -c "$source" -o "$object"
  objects="$objects $object"
done

for test_source in check_test construction_test dispatch_test run_journal_test run_progress_test run_locator_test request_test session_test effect_journal_test effect_inspect_test effect_test report_test version_test; do
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/unit/$test_source.cpp" $objects $core_libs \
    -o "$tmp/$test_source"
  "$tmp/$test_source"
done

# The non-CLI vertical campaign must also survive the direct compiler path.
# shellcheck disable=SC2086
"$cxx" $flags $pipeline_cflags \
  "$srcdir/tests/integration/package_pipeline_test.cpp" \
  $objects $core_libs $pipeline_libs -o "$tmp/package-pipeline-test"
"$tmp/package-pipeline-test"
"$tmp/package-pipeline-test" --failure-matrix
"$tmp/package-pipeline-test" --operation-failure-matrix
"$tmp/package-pipeline-test" --operation-uncertainty-matrix
"$tmp/package-pipeline-test" --operation-lease-loss-matrix
"$tmp/package-pipeline-test" --operation-lease-contention-matrix

for fixture in state_fixture state_inspect_fixture run_store_fixture effect_store_fixture; do
  name=$(printf '%s\n' "${fixture%_fixture}" | tr '_' '-')
  # shellcheck disable=SC2086
  "$cxx" $flags "$srcdir/tests/fixtures/$fixture.cpp" $objects $core_libs \
    -o "$tmp/$name-fixture"
done

# The rootfs feedback oracle is test-only and remains outside pkgctl production.
# shellcheck disable=SC2086
"$cxx" $flags $audit_cflags \
  "$srcdir/tests/fixtures/rootfs_audit_fixture.cpp" \
  $objects $core_libs $audit_libs -o "$tmp/rootfs-audit-fixture"

# shellcheck disable=SC2086
"$cxx" $flags "$srcdir/tests/fixtures/native_target_lock_holder.cpp" \
  $objects $core_libs -o "$tmp/native-target-lock-holder"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/native_target_lock_revoker.cpp" \
  -o "$tmp/native-target-lock-revoker"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/native_credential_context_runner.cpp" \
  -o "$tmp/native-credential-context-runner"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror -fPIC -shared \
  "$srcdir/tests/fixtures/native_credential_context_preload.cpp" \
  -o "$tmp/native-credential-context-preload.so"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/native_runtime_root_fixture.cpp" \
  -o "$tmp/native-runtime-root-fixture"

"$cxx" -nostdlib -static -Wl,-e,_start \
  "$srcdir/tests/fixtures/native_interpreter_x86_64.S" \
  -o "$tmp/native-test-interpreter"

"$cxx" -nostdlib -static -Wl,-e,_start \
  "$srcdir/tests/fixtures/native_lease_loss_interpreter_x86_64.S" \
  -o "$tmp/native-lease-loss-interpreter"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/application_intent_interrupt_fixture.cpp" \
  -o "$tmp/application-intent-interrupt-fixture"
"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/application_intent_interrupt_probe.cpp" \
  -o "$tmp/application-intent-interrupt-probe"
"$srcdir/tests/integration/application_intent_interrupt_fixture_test.sh" \
  "$tmp/application-intent-interrupt-fixture" \
  "$tmp/application-intent-interrupt-probe"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/publication_intent_interrupt_fixture.cpp" \
  -o "$tmp/publication-intent-interrupt-fixture"
"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/publication_intent_interrupt_probe.cpp" \
  -o "$tmp/publication-intent-interrupt-probe"
"$srcdir/tests/integration/publication_intent_interrupt_fixture_test.sh" \
  "$tmp/publication-intent-interrupt-fixture" \
  "$tmp/publication-intent-interrupt-probe"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/publication_terminal_interrupt_fixture.cpp" \
  -o "$tmp/publication-terminal-interrupt-fixture"
"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/publication_terminal_interrupt_probe.cpp" \
  -o "$tmp/publication-terminal-interrupt-probe"
"$srcdir/tests/integration/publication_terminal_interrupt_fixture_test.sh" \
  "$tmp/publication-terminal-interrupt-fixture" \
  "$tmp/publication-terminal-interrupt-probe"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/lifecycle_intent_interrupt_fixture.cpp" \
  -o "$tmp/lifecycle-intent-interrupt-fixture"
"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/lifecycle_intent_interrupt_probe.cpp" \
  -o "$tmp/lifecycle-intent-interrupt-probe"
"$srcdir/tests/integration/lifecycle_intent_interrupt_fixture_test.sh" \
  "$tmp/lifecycle-intent-interrupt-fixture" \
  "$tmp/lifecycle-intent-interrupt-probe"

"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/run_head_interrupt_fixture.cpp" \
  -o "$tmp/run-head-interrupt-fixture"
"$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  "$srcdir/tests/fixtures/artifact_publication_interrupt_fixture.cpp" \
  -o "$tmp/artifact-publication-interrupt-fixture"

"$srcdir/tests/integration/native_root_view_fixture_test.sh" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

# shellcheck disable=SC2086
"$cxx" $flags $cli_cflags \
  "$srcdir/cli/main.cpp" "$srcdir/cli/options.cpp" \
  "$srcdir/cli/run_command.cpp" $objects $core_libs $cli_libs -o "$tmp/pkgctl"
version=$(sed -n 's/^inline constexpr const char\* version_string = "\([^"]*\)";$/\1/p' \
  "$srcdir/include/pkgctl/version.h")
[ -n "$version" ] || {
  echo 'cannot determine pkgctl version from version.h' >&2
  exit 1
}
"$srcdir/tests/integration/cli_readonly_test.sh" "$tmp/pkgctl" "$tmp/state-fixture" \
  "$tmp/run-store-fixture" "$tmp/effect-store-fixture" "$version"

"$srcdir/tests/integration/cli_run_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/native-credential-context-runner" \
  "$tmp/native-credential-context-preload.so" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_construction_only_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_native_construction_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-runtime-root-fixture" \
  "$srcdir/tests/fixtures/collections/native-construction" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_build_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-runtime-root-fixture" \
  "$srcdir/tests/fixtures/collections/native-construction" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

for mode in construction-started artifact-published check-started; do
  "$srcdir/tests/integration/cli_build_process_death_test.sh" "$mode" \
    "$tmp/pkgctl" "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
    "$tmp/native-runtime-root-fixture" "$tmp/run-head-interrupt-fixture" \
    "$tmp/artifact-publication-interrupt-fixture" \
    "$srcdir/tests/fixtures/collections/native-construction" \
    "$srcdir/tests/fixtures/native_root_view_fixture.sh"
done

"$srcdir/tests/integration/cli_run_rootfs_campaign_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-runtime-root-fixture" "$tmp/rootfs-audit-fixture" \
  "$srcdir/tests/fixtures/collections/rootfs-campaign" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_lease_contention_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/native-target-lock-holder" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_lease_loss_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-lease-loss-interpreter" "$tmp/native-target-lock-revoker" \
  "$tmp/native-credential-context-runner" \
  "$tmp/native-credential-context-preload.so" \
  "$srcdir/tests/fixtures/collections/lifecycle-post-install-lease-loss" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_recovery_lease_contention_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/application-intent-interrupt-fixture" \
  "$tmp/native-target-lock-holder" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_removal_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_application_restart_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/application-intent-interrupt-fixture" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_publication_restart_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/publication-intent-interrupt-fixture" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_publication_terminal_restart_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/publication-terminal-interrupt-fixture" \
  "$srcdir/tests/fixtures/collections/simple-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

"$srcdir/tests/integration/cli_run_lifecycle_resolution_test.sh" "$tmp/pkgctl" \
  "$tmp/state-fixture" "$tmp/state-inspect-fixture" \
  "$tmp/native-test-interpreter" "$tmp/lifecycle-intent-interrupt-fixture" \
  "$tmp/native-credential-context-runner" \
  "$tmp/native-credential-context-preload.so" \
  "$srcdir/tests/fixtures/collections/lifecycle-pre-install" \
  "$srcdir/tests/fixtures/collections/lifecycle-post-install" \
  "$srcdir/tests/fixtures/native_root_view_fixture.sh"

for header in "$srcdir"/include/pkgctl/*.h; do
  base=$(basename "$header")
  cat >"$tmp/header.cpp" <<EOF_INNER
#include <pkgctl/$base>
int main() { return 0; }
EOF_INNER
  # shellcheck disable=SC2086
  "$cxx" $flags -fsyntax-only "$tmp/header.cpp"
done

for contract in "$srcdir"/tests/contracts/*.sh; do
  case $(basename "$contract") in
    check_fetch_generation_contract.sh)
      "$contract" "$tmp/pkgctl"
      ;;
    *)
      "$contract" "$srcdir"
      ;;
  esac
done
