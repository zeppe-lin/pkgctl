#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options="$srcdir/cli/options.cpp"
command="$srcdir/cli/run_command.cpp"
execution_report="$srcdir/cli/execution_report.cpp"
execution_report_header="$srcdir/cli/execution_report.h"
main="$srcdir/cli/main.cpp"
meson="$srcdir/cli/meson.build"
tests_meson="$srcdir/tests/meson.build"
integration="$srcdir/tests/integration/cli_run_test.sh"
construction_only_integration="$srcdir/tests/integration/cli_run_construction_only_test.sh"
readonly_integration="$srcdir/tests/integration/cli_readonly_test.sh"
removal_integration="$srcdir/tests/integration/cli_run_removal_test.sh"
restart_integration="$srcdir/tests/integration/cli_run_application_restart_test.sh"
publication_restart_integration="$srcdir/tests/integration/cli_run_publication_restart_test.sh"
publication_terminal_integration="$srcdir/tests/integration/cli_run_publication_terminal_restart_test.sh"
publication_terminal_fixture="$srcdir/tests/fixtures/publication_terminal_interrupt_fixture.cpp"
lifecycle_resolution_integration="$srcdir/tests/integration/cli_run_lifecycle_resolution_test.sh"
lease_contention_integration="$srcdir/tests/integration/cli_run_lease_contention_test.sh"
recovery_lease_contention_integration="$srcdir/tests/integration/cli_run_recovery_lease_contention_test.sh"
lease_loss_integration="$srcdir/tests/integration/cli_run_lease_loss_test.sh"
lifecycle_interrupt_fixture="$srcdir/tests/fixtures/lifecycle_intent_interrupt_fixture.cpp"
interrupt_fixture="$srcdir/tests/fixtures/application_intent_interrupt_fixture.cpp"

for file in "$options" "$command" "$execution_report" "$execution_report_header" "$main" "$meson" "$tests_meson" "$integration" \
            "$construction_only_integration" \
            "$readonly_integration" \
            "$removal_integration" "$restart_integration" "$publication_restart_integration" \
            "$publication_terminal_integration" "$publication_terminal_fixture" \
            "$lifecycle_resolution_integration" "$lease_contention_integration" \
            "$recovery_lease_contention_integration" "$lease_loss_integration" \
            "$lifecycle_interrupt_fixture" "$interrupt_fixture"; do
  [ -s "$file" ] || {
    echo "missing bounded transaction command source: $file" >&2
    exit 1
  }
done

for required in \
  'enum class transaction_run_command_intent' \
  '--start SHA256' \
  '--resume SHA256' \
  '--max-steps N' \
  '--build-parallelism N' \
  '--build-source-date-epoch N' \
  'std::optional<pkgbuild::build_policy> build_policy' \
  'class command_evidence_store final' \
  'pkgbuild::build_policy build_policy' \
  'append_build_policy' \
  'read_build_policy' \
  'policy.identity().hex()' \
  'command evidence build policy identity contradicts retained fields' \
  'retained_evidence->build_policy' \
  'std::optional<transaction_request> transaction' \
  'PKGCTL-COMMAND-EVIDENCE' \
  'pkgctl/command-evidence' \
  'return "command-" + nonce.hex() + ".pce";' \
  'struct retained_native_execution_profiles final' \
  'pkgexec::interpreter_identity interpreter' \
  'append_text(bytes, interpreter.hex())' \
  'retained_evidence->interpreter' \
  'current_execution_backend.get()' \
  'require_native_execution_credentials(' \
  'current interpreter differs from admitted run authority' \
  'native_transaction_resume_execution_scopes(' \
  'admitted_execution_profiles.lifecycle' \
  'pkgexec::encode_backend_capability_profile(' \
  'pkgexec::decode_backend_capability_profile(' \
  'append_transaction_request_inputs' \
  'read_transaction_request_inputs' \
  'command_evidence.load(command.nonce, command.canonical_store)' \
  'canonical_generation_store::open_existing(' \
  'command.canonical_store, binding' \
  '--resume uses retained transaction semantics' \
  'class private_effect_body_store final' \
  'class live_operation_authority final' \
  'public transaction_operation_session_store' \
  'PKGCTL-OPERATION-ARCHIVE-1' \
  'operation-session' \
  'operation-archive' \
  'record.stage() == effect_attempt_stage::application_intent' \
  'application_journals_.load_active(' \
  'result.application_journal->receipt()' \
  'read_optional(' \
  'result.application = std::move(body)' \
  'pkgstate::posix::canonical_generation_store::open_existing(' \
  'native_transaction_requires_target_operation_authority(transaction)' \
  'if (!operation_runtime)' \
  'native_posix_transaction_run_runtime::from_directory_fds(' \
  'require_native_execution_preflight(' \
  'transaction_run_drive_policy::make(command.maximum_steps)' \
  'runtime_path(command, "command-evidence")' \
  'runtime_path(command, "effect-bodies")' \
  'exact transaction run is already admitted; use --resume' \
  'exact transaction run is not admitted; use --start' \
  'retained command evidence recomposes another transaction' \
  'mutation-authority-unavailable' \
  'transaction_run_drive_disposition::mutation_authority_unavailable' \
  'case effectful_operation_outcome::application_resolution_required:' \
  'return "application-resolution-required";' \
  'pkgbuild::environment_policy::hermetic(' \
  'pkgbuild::output_layout_kind::package_root' \
  '0022'; do
  grep -F -- "$required" "$srcdir/cli/options.h" "$options" "$command" \
      >/dev/null || {
    echo "missing bounded transaction command contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  '--source-date-epoch' \
  '--build-file-creation-mask' \
  '--build-output-layout' \
  '--build-locale' \
  '--build-timezone' \
  '--build-network' \
  '--build-home' \
  'command.source_date_epoch' \
  'environment_policy::hermetic(1U'; do
  if grep -F -- "$forbidden" "$srcdir/cli/options.h" "$options" "$command" >/dev/null 2>&1; then
    echo "build policy regressed to scalar/hard-coded command authority: $forbidden" >&2
    exit 1
  fi
done

for required in \
  'failure=' \
  'termination=' \
  'status=' \
  'signal=' \
  'pkgexec::to_string(*failure)' \
  'pkgexec::to_string(termination->kind())'; do
  grep -F -- "$required" "$execution_report" >/dev/null || {
    echo "missing structured execution-failure presentation: $required" >&2
    exit 1
  }
done

for retired_private_format in \
  'PKGCTL-COMMAND-UNIVERSE-3' \
  'pkgctl/command-universe/3' \
  'command-evidence schema v3' \
  'schema v1 or v2' \
  'schemas v1/v2'; do
  if grep -R -F -- "$retired_private_format" "$srcdir/cli" \
      "$srcdir/README.md" "$srcdir/DESIGN.md" "$srcdir/TESTING.md" \
      "$srcdir/MAINTAINING.md" "$srcdir/HISTORY.md" "$srcdir/man" \
      >/dev/null 2>&1; then
    echo "retired private command-evidence history remains: $retired_private_format" >&2
    exit 1
  fi
done

credential_line=$(grep -n -F 'require_native_execution_credentials(' "$command" \
  | tail -1 | cut -d: -f1)
interpreter_line=$(grep -n -F 'interpreter_binding::inspect(command.interpreter)' \
  "$command" | tail -1 | cut -d: -f1)
[ -n "$credential_line" ] && [ -n "$interpreter_line" ] && \
  [ "$credential_line" -lt "$interpreter_line" ] || {
  echo 'current credential authority must be refused before interpreter observation' >&2
  exit 1
}

for forbidden in \
  'create_director' \
  'directory_iterator' \
  'recursive_directory_iterator' \
  'canonical_generation_store::initialize' \
  'sleep(' \
  'usleep(' \
  'std::thread' \
  'std::async' \
  'getenv(' \
  'application_journals_.load(' \
  'latest_' \
  'list_' \
  'PKGCTL-COMMAND-UNIVERSE-1' \
  'PKGCTL-COMMAND-UNIVERSE-2' \
  'pkgctl/command-universe/1' \
  'pkgctl/command-universe/2'; do
  if grep -F -- "$forbidden" "$command" >/dev/null 2>&1; then
    echo "forbidden bounded transaction command policy: $forbidden" >&2
    exit 1
  fi
done

if grep -n -E 'while[[:space:]]*\([^)]*(complete|failed|quiescent|steps)' \
    "$command" >/dev/null 2>&1; then
  echo 'bounded command contains an implicit transaction drive loop' >&2
  exit 1
fi

grep -F "['main.cpp', 'options.cpp', 'execution_report.cpp', 'run_command.cpp']" "$meson" >/dev/null || {
  echo 'bounded run command is not linked into the CLI' >&2
  exit 1
}
grep -F 'execute_transaction_run(std::move(request))' "$main" >/dev/null || {
  echo 'CLI does not dispatch the bounded transaction command' >&2
  exit 1
}
grep -F "'cli-run'" "$tests_meson" >/dev/null || {
  echo 'bounded run command has no process-level integration test' >&2
  exit 1
}
grep -F "'cli-run-construction-only'" "$tests_meson" >/dev/null || {
  echo 'construction-only run has no process-level authority-minimization test' >&2
  exit 1
}

construction_branch_line=$(grep -n -F 'if (!operation_runtime)' "$command" \
  | tail -1 | cut -d: -f1)
lifecycle_open_line=$(grep -n -F 'open_directory(*command.lifecycle_root)' "$command" \
  | tail -1 | cut -d: -f1)
target_open_line=$(grep -n -F 'open_directory(*command.target_root)' "$command" \
  | tail -1 | cut -d: -f1)
[ -n "$construction_branch_line" ] && [ -n "$lifecycle_open_line" ] && \
  [ -n "$target_open_line" ] && \
  [ "$construction_branch_line" -lt "$lifecycle_open_line" ] && \
  [ "$construction_branch_line" -lt "$target_open_line" ] || {
  echo 'target/lifecycle roots are opened before construction-only authority is selected' >&2
  exit 1
}

for sequencing_contract in \
  'auto build_architecture = read_text(bytes, offset);' \
  'auto target_architecture = read_text(bytes, offset);'; do
  grep -F -- "$sequencing_contract" "$command" >/dev/null || {
    echo "retained command evidence decode lacks explicit sequencing: $sequencing_contract" >&2
    exit 1
  }
done

for artifact_reporting_contract in \
  'void render_construction_artifacts(' \
  'bool public_build_frontend' \
  'render_construction_artifacts(' \
  'command.frontend == transaction_run_command_frontend::build'; do
  grep -F -- "$artifact_reporting_contract" "$command" >/dev/null || {
    echo "run construction reporting omits: $artifact_reporting_contract" >&2
    exit 1
  }
done

for construction_only_contract in \
  '--build-architecture x86_64' \
  '--target-architecture fixture-target' \
  "--goal 'build=fixture'" \
  'lifecycle-must-remain-absent' \
  'target-must-remain-absent' \
  'target-locks' \
  'application-journals' \
  'effect-bodies' \
  'require_absent "operation-runtime-$directory"' \
  'construction-only run changed canonical target state' \
  "find \"\$runtime/artifacts\" -type f -name '*.tar'" \
  'rm -rf "$collection"' \
  'durable-steps 0'; do
  grep -F -- "$construction_only_contract" "$construction_only_integration" \
      >/dev/null || {
    echo "missing construction-only CLI qualification: $construction_only_contract" >&2
    exit 1
  }
done

for usage in \
  'pkgctl run OPTIONS --goal SCOPE=SUBJECT [--goal ...] --start SHA256 RUN-AUTHORITY' \
  'pkgctl run --canonical-store PATH --resume SHA256 RUN-AUTHORITY'; do
  grep -F -- "$usage" "$options" >/dev/null || {
    echo "bounded run help omits current usage form: $usage" >&2
    exit 1
  }
  grep -F -- "$usage" "$readonly_integration" >/dev/null || {
    echo "read-only CLI smoke test omits current usage form: $usage" >&2
    exit 1
  }
done

obsolete_usage='pkgctl run OPTIONS --goal SCOPE=SUBJECT [--goal ...] RUN-OPTIONS'
if grep -F -- "$obsolete_usage" "$readonly_integration" >/dev/null 2>&1; then
  echo "read-only CLI smoke test retains obsolete run usage: $obsolete_usage" >&2
  exit 1
fi
grep -F "'cli-run-removal'" "$tests_meson" >/dev/null || {
  echo 'bounded run command has no removal process-level integration test' >&2
  exit 1
}
grep -F "'cli-run-application-restart'" "$tests_meson" >/dev/null || {
  echo 'bounded run command has no application-restart process-level integration test' >&2
  exit 1
}
grep -F "'cli-run-publication-restart'" "$tests_meson" >/dev/null || {
  echo 'bounded run command has no publication-restart process-level integration test' >&2
  exit 1
}
live_authority_gate=$(sed -n \
  '/^capture_native_start_or_skip()/,/^capture_resume()/p' \
  "$lifecycle_resolution_integration")
for required in \
  'run_command --start "$nonce" "$maximum_steps"' \
  'native execution unavailable before transaction execution;' \
  'PKGCTL_REQUIRE_NATIVE_INTEGRATION' \
  'exit 77'; do
  printf '%s\n' "$live_authority_gate" | grep -F -- "$required" >/dev/null || {
    echo "live execution authority case lacks capability-aware skip gate: $required" >&2
    exit 1
  }
done
grep -F -- 'capture_native_start_or_skip start "$run_nonce" 1' \
    "$lifecycle_resolution_integration" >/dev/null || {
  echo 'live execution authority case bypasses capability-aware native start' >&2
  exit 1
}
grep -F "suite: 'integration-privileged'" "$tests_meson" >/dev/null || {
  echo 'native mutating CLI test is not isolated as privileged integration' >&2
  exit 1
}
grep -F 'depends: [native_interpreter, native_credential_context_runner, native_credential_context_preload]' "$tests_meson" >/dev/null || {
  echo 'privileged CLI integration does not build its native interpreter/context fixtures' >&2
  exit 1
}
grep -F 'build_by_default: true' "$tests_meson" >/dev/null || {
  echo 'native interpreter fixture is absent from the default test build graph' >&2
  exit 1
}
grep -F 'PKGCTL_REQUIRE_NATIVE_INTEGRATION' "$integration" >/dev/null || {
  echo 'privileged CLI integration has no release-required mode' >&2
  exit 1
}
for authority in \
  '--build-root' \
  '--lifecycle-root' \
  '--build-user-id' \
  '--build-group-id' \
  '--build-supplementary-group' \
  '--lifecycle-user-id' \
  '--lifecycle-group-id' \
  '--lifecycle-supplementary-group'; do
  grep -F -- "$authority" "$options" "$integration" >/dev/null || {
    echo "missing split run authority: $authority" >&2
    exit 1
  }
done
for obsolete in '--user-id' '--group-id' '--supplementary-group'; do
  if grep -F -- "$obsolete" "$options" "$srcdir/man/pkgctl.1.scd" \
      "$integration" >/dev/null 2>&1; then
    echo "obsolete shared execution authority remains: $obsolete" >&2
    exit 1
  fi
done
grep -F 'construction/check credentials must match the native supervisor' \
    "$command" "$integration" >/dev/null || {
  echo 'construction/check supervisor-credential refusal is not qualified' >&2
  exit 1
}
grep -F 'lifecycle credentials must match the native supervisor' \
    "$command" >/dev/null || {
  echo 'lifecycle supervisor-credential refusal missing' >&2
  exit 1
}
for progress_scope_contract in \
  'live-execution-authority' \
  'credential-refusal' \
  'completed-resume-without-execution-authority' \
  'native_credential_context_runner' \
  'native_credential_context_preload' \
  'capture_command_as' \
  '65534 65534'; do
  grep -F -- "$progress_scope_contract" "$integration" \
      "$lifecycle_resolution_integration" "$tests_meson" >/dev/null || {
    echo "missing progress-scoped resume execution qualification: $progress_scope_contract" >&2
    exit 1
  }
done

for credential_fixture_contract in \
  'PKGCTL_TEST_SUPERVISOR_UID' \
  'PKGCTL_TEST_SUPERVISOR_GID' \
  'PKGCTL_TEST_PREVIOUS_LD_PRELOAD' \
  'native-credential-context-preload' \
  'native_credential_context_preload.cpp'; do
  grep -F -- "$credential_fixture_contract" \
    "$srcdir/tests/fixtures/native_credential_context_runner.cpp" \
    "$srcdir/tests/fixtures/native_credential_context_preload.cpp" \
    "$tests_meson" "$integration" >/dev/null || {
    echo "missing post-loader credential-context qualification: $credential_fixture_contract" >&2
    exit 1
  }
done
if grep -F '::setuid(' "$srcdir/tests/fixtures/native_credential_context_runner.cpp" >/dev/null; then
  echo 'credential-context runner drops credentials before the dynamic loader' >&2
  exit 1
fi

for sanitizer_order_contract in \
  '__asan_init' \
  'dynamic_asan_runtime' \
  'append_preload(preload, dynamic_asan_runtime())' \
  'append_preload(preload, argv[3])' \
  'append_preload(preload, previous_preload)'; do
  grep -F -- "$sanitizer_order_contract" "$srcdir/tests/fixtures/native_credential_context_runner.cpp" >/dev/null || {
    echo "credential-context runner omits sanitizer-safe preload ordering: $sanitizer_order_contract" >&2
    exit 1
  }
done


for forbidden_profile_codec in \
  'append_backend_profile' \
  'read_backend_profile' \
  'execution_guarantee_tag(' \
  'read_execution_guarantee('; do
  if grep -F -- "$forbidden_profile_codec" "$command" >/dev/null 2>&1; then
    echo "run command duplicates libpkgexec backend-profile codec: $forbidden_profile_codec" >&2
    exit 1
  fi
done

preflight_line=$(grep -n -F 'require_native_execution_preflight(' \
  "$command" | tail -n 1 | cut -d: -f1)
retain_line=$(grep -n -F 'command_evidence.retain(' \
  "$command" | head -n 1 | cut -d: -f1)
[ -n "$preflight_line" ] && [ -n "$retain_line" ] && \
    [ "$preflight_line" -lt "$retain_line" ] || {
  echo 'native execution preflight does not precede command-evidence retention' >&2
  exit 1
}
for retained_resume_contract in \
  'inject_resume_semantics' \
  '--resume uses retained transaction semantics' \
  'rm -rf "$collection"'; do
  grep -F -- "$retained_resume_contract" "$integration" >/dev/null || {
    echo "missing retained resume-semantics qualification: $retained_resume_contract" >&2
    exit 1
  }
done

for resume_script in \
  "$integration" \
  "$removal_integration" \
  "$restart_integration" \
  "$publication_restart_integration" \
  "$publication_terminal_integration" \
  "$lifecycle_resolution_integration" \
  "$lease_contention_integration" \
  "$recovery_lease_contention_integration" \
  "$lease_loss_integration"; do
  grep -F 'if [ "$intent" = --start ]; then' "$resume_script" >/dev/null || {
    echo "resume-capable integration does not isolate start semantics: $resume_script" >&2
    exit 1
  }
done

for required_test in \
  'disposition step-limit-reached' \
  'durable-steps 1' \
  'exact transaction run is already admitted; use --resume' \
  'native execution unavailable before transaction execution;' \
  'unsupported native execution retained transaction evidence before refusal' \
  'rm -rf "$collection"' \
  'origin resumed' \
  'disposition completed' \
  'package fixture 1.0-1' \
  'durable-steps 0'; do
  grep -F -- "$required_test" "$integration" >/dev/null || {
    echo "missing process-level run qualification: $required_test" >&2
    exit 1
  }
done

for required_removal_test in \
  '--converge-exact' \
  "--goal 'build=fixture'" \
  'action=remove' \
  'rm -rf "$collection"' \
  'effect.application-outcome=completed' \
  'effect.application-completed-evidence=' \
  'effect.publication-outcome=published' \
  'effect.terminal-outcome=completed' \
  'packages 0' \
  'durable-steps 0'; do
  grep -F -- "$required_removal_test" "$removal_integration" >/dev/null || {
    echo "missing process-level removal qualification: $required_removal_test" >&2
    exit 1
  }
done

for required_restart_test in \
  'effect.stage=application-intent' \
  'effect.disposition=resume-application' \
  'active-request-v1-sha256-*.ref' \
  'operation-session-*.bin' \
  'operation-archive-*.bin' \
  'started operation lacks retained session authority' \
  'missing-session-run-record' \
  'missing-archive-run-record' \
  'effect.application-journal=$application_journal' \
  'not-an-application-journal-identity' \
  'durable-steps 0'; do
  grep -F -- "$required_restart_test" "$restart_integration" >/dev/null || {
    echo "missing process-level application-restart qualification: $required_restart_test" >&2
    exit 1
  }
done

for required_publication_terminal in \
  'effect.stage=publication-terminal' \
  'effect.disposition=seal-terminal' \
  'publication-receipt-*.bin' \
  'private run object is absent:' \
  'missing-receipt-effect-record' \
  'not-an-application-journal-identity' \
  'state-not-republished' \
  'durable-steps 0'; do
  grep -F -- "$required_publication_terminal" "$publication_terminal_integration" >/dev/null || {
    echo "missing process-level publication-terminal restart qualification: $required_publication_terminal" >&2
    exit 1
  }
done

for required_publication_terminal_interrupt in \
  'PTRACE_TRACEME' \
  'PTRACE_SYSCALL' \
  'SYS_renameat' \
  'SYS_fsync' \
  '.tmp-effect-head-' \
  '.pjeh' \
  'SIGKILL'; do
  grep -F -- "$required_publication_terminal_interrupt" "$publication_terminal_fixture" >/dev/null || {
    echo "missing external publication-terminal interruption mechanism: $required_publication_terminal_interrupt" >&2
    exit 1
  }
done

for required_lifecycle_resolution in \
  "'lifecycle:pre-install=fixture'" \
  "'lifecycle:post-install=fixture'" \
  '--goal "$lifecycle_goal"' \
  'expected_stage=before-lifecycle-intent' \
  'expected_stage=after-lifecycle-intent' \
  'effect.stage=$expected_stage' \
  'effect.disposition=external-resolution-required' \
  'effect.automatically-continuable=false' \
  'disposition external-resolution-required' \
  'durable-steps 0' \
  'lifecycle-sessions' \
  'rm -rf "$collection"' \
  'repeat-resume'; do
  grep -F -- "$required_lifecycle_resolution" "$lifecycle_resolution_integration" >/dev/null || {
    echo "missing process-level lifecycle-resolution qualification: $required_lifecycle_resolution" >&2
    exit 1
  }
done

for required_lifecycle_interrupt in \
  'PTRACE_TRACEME' \
  'PTRACE_SYSCALL' \
  'SYS_renameat' \
  'SYS_fsync' \
  '.tmp-effect-head-' \
  'before-lifecycle-intent' \
  'after-lifecycle-intent' \
  'SIGKILL'; do
  grep -F -- "$required_lifecycle_interrupt" "$lifecycle_interrupt_fixture" >/dev/null || {
    echo "missing external lifecycle-intent interruption mechanism: $required_lifecycle_interrupt" >&2
    exit 1
  }
done

for required_interrupt in \
  'PTRACE_TRACEME' \
  'PTRACE_SYSCALL' \
  'SYS_fsync' \
  'active-request-v1-sha256-' \
  'SIGKILL'; do
  grep -F -- "$required_interrupt" "$interrupt_fixture" >/dev/null || {
    echo "missing external application-intent interruption mechanism: $required_interrupt" >&2
    exit 1
  }
done

if grep -R -F 'PTRACE_' "$srcdir/cli" "$srcdir/src" "$srcdir/include" \
    >/dev/null 2>&1; then
  echo 'production controller contains a test-process ptrace hook' >&2
  exit 1
fi


for documented in \
  'Release 0.35.0 closes the functional package-management chain' \
  'Release 0.35.0 bounded native command boundary' \
  'Release 0.35.0 bounded native command qualification' \
  'Version 0.40.4 requires libpkgapply-posix 3.2.3' \
  'Version 0.40.3 preserves subordinate application uncertainty' \
  'Version 0.40.2 requires libpkgapply-posix 3.2.2' \
  'Version 0.40.1 requires libpkgtransaction 4.1.0' \
  'Version 0.40.0 makes native target-operation policy explicit start-only' \
  'Version 0.39.0 phase-separates concrete construction and check inputs while' \
  'Version 0.38.0 makes durable construction and check ownership recoverable' \
  'Version 0.37.0 retains the native catalog' \
  'Release 0.36.0 construction-only runtime authority' \
  'Release 0.36.0 construction-only authority qualification' \
  'BOUNDED NATIVE TRANSACTION COMMAND' \
  'current private command-evidence format' \
  'retained transaction semantics'; do
  grep -F -- "$documented" "$srcdir/README.md" "$srcdir/DESIGN.md" \
      "$srcdir/TESTING.md" "$srcdir/man/pkgctl.1.scd" \
      "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null || {
    echo "missing bounded transaction command documentation: $documented" >&2
    exit 1
  }
done

for interpreter_recovery_proof in \
  'interpreter_override=/bin/false' \
  'current interpreter differs from admitted run authority' \
  'live-authority-interpreter-run-head'; do
  grep -F -- "$interpreter_recovery_proof" "$lifecycle_resolution_integration" \
      >/dev/null || {
    echo "missing retained-interpreter live-authority proof: $interpreter_recovery_proof" >&2
    exit 1
  }
done
for operation_only_recovery_proof in \
  'interpreter_override=/bin/false' \
  'capture_run resume 0 --resume'; do
  grep -F -- "$operation_only_recovery_proof" "$integration" >/dev/null || {
    echo "missing operation-only resume proof without process authority: $operation_only_recovery_proof" >&2
    exit 1
  }
done

# Completed lifecycle bodies own the exact backend-profile bytes that produced them.
! grep -F 'lifecycle_capabilities_' "$command" >/dev/null || {
  echo 'command effect-body store still injects command-level lifecycle profile into historical decode' >&2
  exit 1
}
grep -F 'session.before()[index]);' "$command" >/dev/null || {
  echo 'pre-lifecycle body decode does not consume only retained body plus exact admitted session' >&2
  exit 1
}
grep -F 'session.after()[index]);' "$command" >/dev/null || {
  echo 'post-lifecycle body decode does not consume only retained body plus exact admitted session' >&2
  exit 1
}
