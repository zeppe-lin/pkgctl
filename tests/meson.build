# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  check_test = executable(
    'check-test',
    'unit/check_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('check', check_test, suite: 'unit')

  construction_test = executable(
    'construction-test',
    'unit/construction_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('construction', construction_test, suite: 'unit')

  preparation_policy_test = executable(
    'preparation-policy-test',
    'unit/preparation_policy_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('preparation-policy', preparation_policy_test, suite: 'unit')

  dispatch_test = executable(
    'dispatch-test',
    'unit/dispatch_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('dispatch', dispatch_test, suite: 'unit')

  run_journal_test = executable(
    'run-journal-test',
    'unit/run_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-journal', run_journal_test, suite: 'unit')

  run_progress_test = executable(
    'run-progress-test',
    'unit/run_progress_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-progress', run_progress_test, suite: 'unit')

  run_locator_test = executable(
    'run-locator-test',
    'unit/run_locator_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-locator', run_locator_test, suite: 'unit')

  request_test = executable(
    'request-test',
    'unit/request_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('request', request_test, suite: 'unit')

  session_test = executable(
    'session-test',
    'unit/session_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('session', session_test, suite: 'unit')

  package_pipeline_reconcile_deps = [
    dependency(
      'libpkgreconcile',
      version: ['>=0.3.0', '<1.0.0'],
      required: true,
      static: want_static,
    ),
    dependency(
      'libpkgreconcile-apply',
      version: ['>=0.1.0', '<1.0.0'],
      required: true,
      static: want_static,
    ),
    dependency(
      'libpkgreconcile-posix',
      version: ['>=0.1.0', '<1.0.0'],
      required: true,
      static: want_static,
    ),
    dependency(
      'libpkgreconcile-apply-posix',
      version: ['>=0.1.0', '<1.0.0'],
      required: true,
      static: want_static,
    ),
  ]
  package_pipeline_test = executable(
    'package-pipeline-test',
    'integration/package_pipeline_test.cpp',
    dependencies: [pkgctl_core_dep] + package_pipeline_reconcile_deps,
  )
  test('package-pipeline', package_pipeline_test, suite: 'integration')
  test(
    'package-failure-matrix',
    package_pipeline_test,
    args: ['--failure-matrix'],
    suite: 'integration',
  )
  test(
    'package-operation-failure-matrix',
    package_pipeline_test,
    args: ['--operation-failure-matrix'],
    suite: 'integration',
  )
  test(
    'package-operation-uncertainty-matrix',
    package_pipeline_test,
    args: ['--operation-uncertainty-matrix'],
    suite: 'integration',
  )
  test(
    'package-operation-lease-loss-matrix',
    package_pipeline_test,
    args: ['--operation-lease-loss-matrix'],
    suite: 'integration',
  )
  test(
    'package-operation-lease-contention-matrix',
    package_pipeline_test,
    args: ['--operation-lease-contention-matrix'],
    suite: 'integration',
  )

  effect_journal_test = executable(
    'effect-journal-test',
    'unit/effect_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-journal', effect_journal_test, suite: 'unit')

  effect_inspect_test = executable(
    'effect-inspect-test',
    'unit/effect_inspect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-inspect', effect_inspect_test, suite: 'unit')

  effect_test = executable(
    'effect-test',
    'unit/effect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect', effect_test, suite: 'unit')

  report_test = executable(
    'report-test',
    'unit/report_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('report', report_test, suite: 'unit')

  version_test = executable(
    'version-test',
    'unit/version_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('version', version_test, suite: 'unit')

  state_fixture = executable(
    'state-fixture',
    'fixtures/state_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  run_store_fixture = executable(
    'run-store-fixture',
    'fixtures/run_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  effect_store_fixture = executable(
    'effect-store-fixture',
    'fixtures/effect_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  state_inspect_fixture = executable(
    'state-inspect-fixture',
    'fixtures/state_inspect_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  libpkgaudit_test_dep = dependency(
    'libpkgaudit',
    version: ['>=0.1.0', '<1.0.0'],
    required: true,
    static: want_static,
  )
  rootfs_audit_fixture = executable(
    'rootfs-audit-fixture',
    'fixtures/rootfs_audit_fixture.cpp',
    dependencies: [pkgctl_core_dep, libpkgaudit_test_dep],
  )

  native_target_lock_holder = executable(
    'native-target-lock-holder',
    'fixtures/native_target_lock_holder.cpp',
    dependencies: pkgctl_core_dep,
  )

  native_target_lock_revoker = executable(
    'native-target-lock-revoker',
    'fixtures/native_target_lock_revoker.cpp',
  )

  native_credential_context_runner = executable(
    'native-credential-context-runner',
    'fixtures/native_credential_context_runner.cpp',
  )

  native_credential_context_preload = shared_module(
    'native-credential-context-preload',
    'fixtures/native_credential_context_preload.cpp',
    name_prefix: '',
    install: false,
    override_options: ['b_sanitize=none'],
  )

  native_runtime_root_fixture = executable(
    'native-runtime-root-fixture',
    'fixtures/native_runtime_root_fixture.cpp',
  )

  cpp = meson.get_compiler('cpp')
  native_interpreter = custom_target(
    'native-test-interpreter',
    input: 'fixtures/native_interpreter_x86_64.S',
    output: 'native-test-interpreter',
    build_by_default: true,
    command: cpp.cmd_array() + [
      '-nostdlib',
      '-static',
      '-Wl,-e,_start',
      '@INPUT@',
      '-o',
      '@OUTPUT@',
    ],
  )

  native_lease_loss_interpreter = custom_target(
    'native-lease-loss-interpreter',
    input: 'fixtures/native_lease_loss_interpreter_x86_64.S',
    output: 'native-lease-loss-interpreter',
    build_by_default: true,
    command: cpp.cmd_array() + [
      '-nostdlib',
      '-static',
      '-Wl,-e,_start',
      '@INPUT@',
      '-o',
      '@OUTPUT@',
    ],
  )

  application_intent_interrupt_fixture = executable(
    'application-intent-interrupt-fixture',
    'fixtures/application_intent_interrupt_fixture.cpp',
  )

  application_intent_interrupt_probe = executable(
    'application-intent-interrupt-probe',
    'fixtures/application_intent_interrupt_probe.cpp',
  )

  publication_intent_interrupt_fixture = executable(
    'publication-intent-interrupt-fixture',
    'fixtures/publication_intent_interrupt_fixture.cpp',
  )

  publication_intent_interrupt_probe = executable(
    'publication-intent-interrupt-probe',
    'fixtures/publication_intent_interrupt_probe.cpp',
  )

  publication_terminal_interrupt_fixture = executable(
    'publication-terminal-interrupt-fixture',
    'fixtures/publication_terminal_interrupt_fixture.cpp',
  )

  publication_terminal_interrupt_probe = executable(
    'publication-terminal-interrupt-probe',
    'fixtures/publication_terminal_interrupt_probe.cpp',
  )

  lifecycle_intent_interrupt_fixture = executable(
    'lifecycle-intent-interrupt-fixture',
    'fixtures/lifecycle_intent_interrupt_fixture.cpp',
  )

  lifecycle_intent_interrupt_probe = executable(
    'lifecycle-intent-interrupt-probe',
    'fixtures/lifecycle_intent_interrupt_probe.cpp',
  )

  application_intent_interrupt_fixture_test = find_program(
    'integration/application_intent_interrupt_fixture_test.sh',
  )
  test(
    'application-intent-interrupt-fixture',
    application_intent_interrupt_fixture_test,
    args: [
      application_intent_interrupt_fixture,
      application_intent_interrupt_probe,
    ],
    depends: [
      application_intent_interrupt_fixture,
      application_intent_interrupt_probe,
    ],
    suite: 'integration',
  )

  publication_intent_interrupt_fixture_test = find_program(
    'integration/publication_intent_interrupt_fixture_test.sh',
  )
  test(
    'publication-intent-interrupt-fixture',
    publication_intent_interrupt_fixture_test,
    args: [
      publication_intent_interrupt_fixture,
      publication_intent_interrupt_probe,
    ],
    depends: [
      publication_intent_interrupt_fixture,
      publication_intent_interrupt_probe,
    ],
    suite: 'integration',
  )

  publication_terminal_interrupt_fixture_test = find_program(
    'integration/publication_terminal_interrupt_fixture_test.sh',
  )
  test(
    'publication-terminal-interrupt-fixture',
    publication_terminal_interrupt_fixture_test,
    args: [
      publication_terminal_interrupt_fixture,
      publication_terminal_interrupt_probe,
    ],
    depends: [
      publication_terminal_interrupt_fixture,
      publication_terminal_interrupt_probe,
    ],
    suite: 'integration',
  )

  lifecycle_intent_interrupt_fixture_test = find_program(
    'integration/lifecycle_intent_interrupt_fixture_test.sh',
  )
  test(
    'lifecycle-intent-interrupt-fixture',
    lifecycle_intent_interrupt_fixture_test,
    args: [
      lifecycle_intent_interrupt_fixture,
      lifecycle_intent_interrupt_probe,
    ],
    depends: [
      lifecycle_intent_interrupt_fixture,
      lifecycle_intent_interrupt_probe,
    ],
    suite: 'integration',
  )

  cli_readonly_test = find_program('integration/cli_readonly_test.sh')
  test(
    'cli-readonly',
    cli_readonly_test,
    args: [
      pkgctl_exe,
      state_fixture,
      run_store_fixture,
      effect_store_fixture,
      meson.project_version(),
    ],
    suite: 'integration',
  )

  cli_run_test = find_program('integration/cli_run_test.sh')
  test(
    'cli-run',
    cli_run_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      native_credential_context_runner,
      native_credential_context_preload.full_path(),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_interpreter, native_credential_context_runner, native_credential_context_preload],
    suite: 'integration-privileged',
  )

  cli_run_construction_only_test = find_program(
    'integration/cli_run_construction_only_test.sh',
  )
  test(
    'cli-run-construction-only',
    cli_run_construction_only_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: native_interpreter,
    suite: 'integration-privileged',
  )

  cli_run_native_construction_test = find_program(
    'integration/cli_run_native_construction_test.sh',
  )
  test(
    'cli-run-native-construction',
    cli_run_native_construction_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_runtime_root_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/native-construction',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: native_runtime_root_fixture,
    suite: 'integration-privileged',
  )

  cli_build_test = find_program(
    'integration/cli_build_test.sh',
  )
  test(
    'cli-build',
    cli_build_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_runtime_root_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/native-construction',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: native_runtime_root_fixture,
    suite: 'integration-privileged',
  )

  cli_run_rootfs_campaign_test = find_program(
    'integration/cli_run_rootfs_campaign_test.sh',
  )
  test(
    'cli-run-rootfs-campaign',
    cli_run_rootfs_campaign_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_runtime_root_fixture,
      rootfs_audit_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/rootfs-campaign',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_runtime_root_fixture, rootfs_audit_fixture],
    suite: 'integration-privileged',
  )

  cli_run_lease_contention_test = find_program(
    'integration/cli_run_lease_contention_test.sh',
  )
  test(
    'cli-run-lease-contention',
    cli_run_lease_contention_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      native_target_lock_holder,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_interpreter, native_target_lock_holder],
    suite: 'integration-privileged',
  )

  cli_run_lease_loss_test = find_program(
    'integration/cli_run_lease_loss_test.sh',
  )
  test(
    'cli-run-lease-loss',
    cli_run_lease_loss_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_lease_loss_interpreter,
      native_target_lock_revoker,
      native_credential_context_runner,
      native_credential_context_preload.full_path(),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/lifecycle-post-install-lease-loss',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_lease_loss_interpreter, native_target_lock_revoker, native_credential_context_runner, native_credential_context_preload],
    suite: 'integration-privileged',
  )

  cli_run_recovery_lease_contention_test = find_program(
    'integration/cli_run_recovery_lease_contention_test.sh',
  )
  test(
    'cli-run-recovery-lease-contention',
    cli_run_recovery_lease_contention_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      application_intent_interrupt_fixture,
      native_target_lock_holder,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [
      native_interpreter,
      application_intent_interrupt_fixture,
      native_target_lock_holder,
    ],
    suite: 'integration-privileged',
  )

  cli_run_removal_test = find_program('integration/cli_run_removal_test.sh')
  test(
    'cli-run-removal',
    cli_run_removal_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: native_interpreter,
    suite: 'integration-privileged',
  )

  cli_run_application_restart_test = find_program(
    'integration/cli_run_application_restart_test.sh',
  )
  test(
    'cli-run-application-restart',
    cli_run_application_restart_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      application_intent_interrupt_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_interpreter, application_intent_interrupt_fixture],
    suite: 'integration-privileged',
  )

  cli_run_publication_restart_test = find_program(
    'integration/cli_run_publication_restart_test.sh',
  )
  test(
    'cli-run-publication-restart',
    cli_run_publication_restart_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      publication_intent_interrupt_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_interpreter, publication_intent_interrupt_fixture],
    suite: 'integration-privileged',
  )

  cli_run_publication_terminal_restart_test = find_program(
    'integration/cli_run_publication_terminal_restart_test.sh',
  )
  test(
    'cli-run-publication-terminal-restart',
    cli_run_publication_terminal_restart_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      publication_terminal_interrupt_fixture,
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/simple-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [native_interpreter, publication_terminal_interrupt_fixture],
    suite: 'integration-privileged',
  )

  cli_run_lifecycle_resolution_test = find_program(
    'integration/cli_run_lifecycle_resolution_test.sh',
  )
  test(
    'cli-run-lifecycle-resolution',
    cli_run_lifecycle_resolution_test,
    args: [
      pkgctl_exe,
      state_fixture,
      state_inspect_fixture,
      native_interpreter,
      lifecycle_intent_interrupt_fixture,
      native_credential_context_runner,
      native_credential_context_preload.full_path(),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/lifecycle-pre-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/collections/lifecycle-post-install',
      ),
      join_paths(
        meson.project_source_root(),
        'tests/fixtures/native_root_view_fixture.sh',
      ),
    ],
    depends: [
      native_interpreter,
      lifecycle_intent_interrupt_fixture,
      native_credential_context_runner,
      native_credential_context_preload,
    ],
    suite: 'integration-privileged',
  )

  pkgctl_headers = [
    'check.h',
    'check_codec.h',
    'construction.h',
    'construction_codec.h',
    'dispatch.h',
    'controller.h',
    'effect.h',
    'effect_inspect.h',
    'effect_journal.h',
    'effect_journal_codec.h',
    'effect_restart.h',
    'effect_store.h',
    'error.h',
    'identity.h',
    'operation_codec.h',
    'pkgctl.h',
    'preparation.h',
    'progression.h',
    'report.h',
    'request.h',
    'run_admit.h',
    'run_authority.h',
    'run_advance.h',
    'run_drive.h',
    'run_evidence.h',
    'run_evidence_codec.h',
    'run_evidence_store.h',
    'run_journal.h',
    'run_journal_codec.h',
    'run_inspect.h',
    'run_launch.h',
    'run_locator.h',
    'run_native.h',
    'run_nonce.h',
    'run_operation.h',
    'run_reconcile.h',
    'run_recovery.h',
    'run_progress.h',
    'run_commit.h',
    'run_execute.h',
    'run_restart.h',
    'run_runtime.h',
    'run_store.h',
    'session.h',
    'target_observation.h',
    'version.h',
  ]
  foreach header : pkgctl_headers
    header_test = executable(
      'public-header-' + header.underscorify(),
      'unit/public_header_test.cpp',
      cpp_args: ['-DPKGCTL_TEST_HEADER="pkgctl/' + header + '"'],
      dependencies: pkgctl_core_dep,
    )
    test('public-header-' + header, header_test, suite: 'header')
  endforeach

  package_pipeline_contract = find_program(
    'contracts/check_package_pipeline_contract.sh',
  )
  test(
    'package-pipeline-contract',
    package_pipeline_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  package_fault_matrix_contract = find_program(
    'contracts/check_package_fault_matrix_contract.sh',
  )
  test(
    'package-fault-matrix-contract',
    package_fault_matrix_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  target_observation_contract = find_program(
    'contracts/check_target_observation_contract.sh',
  )
  test(
    'target-observation-contract',
    target_observation_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  construction_contract = find_program('contracts/check_construction_contract.sh')
  test('construction-contract', construction_contract, args: [meson.project_source_root()], suite: 'contract')

  preparation_contract = find_program('contracts/check_preparation_contract.sh')
  test('preparation-contract', preparation_contract, args: [meson.project_source_root()], suite: 'contract')

  progression_contract = find_program('contracts/check_progression_contract.sh')
  test('progression-contract', progression_contract, args: [meson.project_source_root()], suite: 'contract')

  check_contract = find_program('contracts/check_check_contract.sh')
  test('check-contract', check_contract, args: [meson.project_source_root()], suite: 'contract')

  dispatch_contract = find_program('contracts/check_dispatch_contract.sh')
  test('dispatch-contract', dispatch_contract, args: [meson.project_source_root()], suite: 'contract')

  run_journal_contract = find_program('contracts/check_run_journal_contract.sh')
  test('run-journal-contract', run_journal_contract, args: [meson.project_source_root()], suite: 'contract')

  run_execute_contract = find_program('contracts/check_run_execute_contract.sh')
  test('run-execute-contract', run_execute_contract, args: [meson.project_source_root()], suite: 'contract')

  run_evidence_contract = find_program('contracts/check_run_evidence_contract.sh')
  test(
    'run-evidence-contract',
    run_evidence_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_recovery_contract = find_program('contracts/check_run_recovery_contract.sh')
  test(
    'run-recovery-contract',
    run_recovery_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_progress_contract = find_program('contracts/check_run_progress_contract.sh')
  test(
    'run-progress-contract',
    run_progress_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_reconcile_contract = find_program('contracts/check_run_reconcile_contract.sh')
  test('run-reconcile-contract', run_reconcile_contract, args: [meson.project_source_root()], suite: 'contract')

  run_admit_contract = find_program('contracts/check_run_admit_contract.sh')
  test('run-admit-contract', run_admit_contract, args: [meson.project_source_root()], suite: 'contract')

  run_authority_contract = find_program('contracts/check_run_authority_contract.sh')
  test('run-authority-contract', run_authority_contract, args: [meson.project_source_root()], suite: 'contract')

  run_locator_contract = find_program('contracts/check_run_locator_contract.sh')
  test(
    'run-locator-contract',
    run_locator_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_advance_contract = find_program('contracts/check_run_advance_contract.sh')
  test('run-advance-contract', run_advance_contract, args: [meson.project_source_root()], suite: 'contract')

  run_driver_source_contract = find_program(
    'contracts/check_run_driver_source_contract.sh',
  )
  test(
    'run-driver-source-contract',
    run_driver_source_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_operation_contract = find_program('contracts/check_run_operation_contract.sh')
  test(
    'run-operation-contract',
    run_operation_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_native_contract = find_program('contracts/check_run_native_contract.sh')
  test(
    'run-native-contract',
    run_native_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_nonce_contract = find_program('contracts/check_run_nonce_contract.sh')
  test(
    'run-nonce-contract',
    run_nonce_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_runtime_contract = find_program('contracts/check_run_runtime_contract.sh')
  test(
    'run-runtime-contract',
    run_runtime_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_drive_contract = find_program('contracts/check_run_drive_contract.sh')
  test('run-drive-contract', run_drive_contract, args: [meson.project_source_root()], suite: 'contract')

  run_launch_contract = find_program('contracts/check_run_launch_contract.sh')
  test('run-launch-contract', run_launch_contract, args: [meson.project_source_root()], suite: 'contract')

  effect_inspect_contract = find_program('contracts/check_effect_inspect_contract.sh')
  test(
    'effect-inspect-contract',
    effect_inspect_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  effect_inspect_cli_contract = find_program(
    'contracts/check_effect_inspect_cli_contract.sh',
  )
  test(
    'effect-inspect-cli-contract',
    effect_inspect_cli_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_native_construction_contract = find_program(
    'contracts/check_cli_native_construction_contract.sh',
  )
  test(
    'cli-native-construction-contract',
    cli_native_construction_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_build_contract = find_program(
    'contracts/check_cli_build_contract.sh',
  )
  test(
    'cli-build-contract',
    cli_build_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_run_rootfs_campaign_contract = find_program(
    'contracts/check_cli_run_rootfs_campaign_contract.sh',
  )
  test(
    'cli-run-rootfs-campaign-contract',
    cli_run_rootfs_campaign_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_lease_contention_contract = find_program(
    'contracts/check_cli_lease_contention_contract.sh',
  )
  test(
    'cli-lease-contention-contract',
    cli_lease_contention_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_lease_loss_contract = find_program(
    'contracts/check_cli_lease_loss_contract.sh',
  )
  test(
    'cli-lease-loss-contract',
    cli_lease_loss_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  cli_recovery_lease_contention_contract = find_program(
    'contracts/check_cli_recovery_lease_contention_contract.sh',
  )
  test(
    'cli-recovery-lease-contention-contract',
    cli_recovery_lease_contention_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_command_contract = find_program('contracts/check_run_command_contract.sh')
  test(
    'run-command-contract',
    run_command_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_inspect_contract = find_program('contracts/check_run_inspect_contract.sh')
  test('run-inspect-contract', run_inspect_contract, args: [meson.project_source_root()], suite: 'contract')

  run_inspect_cli_contract = find_program('contracts/check_run_inspect_cli_contract.sh')
  test(
    'run-inspect-cli-contract',
    run_inspect_cli_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  source_contract = find_program('contracts/check_source_contract.sh')
  test('source-contract', source_contract, args: [meson.project_source_root()], suite: 'contract')

  cli_yaml_boundary = find_program('contracts/check_cli_yaml_boundary_contract.sh')
  test(
    'cli-yaml-boundary-contract',
    cli_yaml_boundary,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  effect_contract = find_program('contracts/check_effect_contract.sh')
  test('effect-contract', effect_contract, args: [meson.project_source_root()], suite: 'contract')

  restart_contract = find_program('contracts/check_restart_contract.sh')
  test('restart-contract', restart_contract, args: [meson.project_source_root()], suite: 'contract')

  manual_contract = find_program('contracts/check_manual_contract.sh')
  test('manual-contract', manual_contract, args: [meson.project_source_root()], suite: 'contract')

  test_layout_contract = find_program('contracts/check_test_layout_contract.sh')
  test(
    'test-layout-contract',
    test_layout_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  repository_contract = find_program('contracts/check_repository_contract.sh')
  test(
    'repository-contract',
    repository_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  documentation_source_contract = find_program(
    'contracts/check_documentation_source.sh',
  )
  test(
    'documentation-source-contract',
    documentation_source_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  release_metadata = find_program('contracts/check_release_metadata.sh')
  test('release-metadata', release_metadata, args: [meson.project_source_root()], suite: 'contract')

  if not want_static
    source_generation_contract = find_program(
      'contracts/check_source_generation_contract.sh',
    )
    test(
      'source-generation-contract',
      source_generation_contract,
      args: [pkgctl_exe],
      suite: 'contract',
    )

    fetch_generation_contract = find_program(
      'contracts/check_fetch_generation_contract.sh',
    )
    test(
      'fetch-generation-contract',
      fetch_generation_contract,
      args: [pkgctl_exe],
      suite: 'contract',
    )
  endif
endif
