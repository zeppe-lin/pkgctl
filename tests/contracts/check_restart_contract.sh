#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
model="$srcdir/include/pkgctl/effect_journal.h"
model_source="$srcdir/src/effect_journal.cpp"
codec_header="$srcdir/include/pkgctl/effect_journal_codec.h"
restart="$srcdir/src/effect_restart.cpp"
effect="$srcdir/src/effect.cpp"
store="$srcdir/src/effect_store.cpp"
codec="$srcdir/src/effect_journal_codec.cpp"

for file in "$model" "$model_source" "$codec_header" "$restart" \
            "$effect" "$store" "$codec"; do
  [ -s "$file" ] || {
    echo "missing durable effect authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class effect_attempt_record final' \
  'effect_attempt_record::admit' \
  'effect_attempt_record::begin_application' \
  'effect_attempt_record::begin_publication' \
  'effect_attempt_record::seal_terminal' \
  'effect_attempt_record::validate_successor_of' \
  'execute_effectful_operation_durable' \
  'transaction_effect_body_sink' \
  'bodies->retain_lifecycle(result)' \
  'bodies->retain_application(request.application(), application)' \
  'bodies->retain_publication_request(publication_request)' \
  'bodies->retain_publication_receipt(' \
  'resume_effectful_operation' \
  'effect_restart_requires_continuation_driver' \
  'effect_restart_requires_publication_driver' \
  'validate_ahead_application_receipt' \
  'pkgapply::application_restart_disposition::terminal' \
  'application && !journal.application()' \
  'journal.complete_application(*application)' \
  'physical.resume_application' \
  'physical.publication_state_projection()' \
  'state.read_state()' \
  'state.publish_state' \
  'effect_restart_disposition::external_resolution_required' \
  'effect_attempt_encoding_version = 1' \
  'pkgctl/effect-journal-head/1' \
  'head_magic' \
  'record_encoding_version' \
  'read_head' \
  'publish_head' \
  'verify_existing_snapshot' \
  'effect-attempt journal sequence disagrees with retained history' \
  'lease-loss terminal record follows unresolved publication intent' \
  'latest->identity() == record.identity()' \
  'effect-journal head names a missing snapshot' \
  'posix_effect_journal_store::from_directory_fd' \
  '::rewinddir' \
  '::linkat' \
  '::fsync' \
  'O_NOFOLLOW'; do
  grep -F -- "$required" "$model" "$model_source" "$codec_header" \
      "$restart" "$effect" "$store" "$codec" \
      >/dev/null || {
    echo "missing durable effect contract: $required" >&2
    exit 1
  }
done

first_after()
{
  start=$1
  token=$2
  awk -v start="$start" -v token="$token" '
    index($0, start) { active = 1 }
    active && index($0, token) { print NR; exit }
  ' "$effect"
}

check_order()
{
  start=$1
  intent=$2
  call=$3
  intent_line=$(first_after "$start" "$intent")
  call_line=$(first_after "$start" "$call")
  [ -n "$intent_line" ] && [ -n "$call_line" ] &&
      [ "$intent_line" -lt "$call_line" ] || {
    echo "durable effect does not persist $intent before $call" >&2
    exit 1
  }
}

check_order execute_effectful_operation_durable \
  journal.begin_before driver.execute_lifecycle
check_order execute_effectful_operation_durable \
  journal.begin_application driver.apply_application
check_order execute_effectful_operation_durable \
  journal.begin_after 'driver.execute_lifecycle(session.after'
check_order execute_effectful_operation_durable \
  journal.begin_publication driver.publish_state

check_order execute_effectful_operation_durable \
  'bodies->retain_lifecycle(result)' 'journal.complete_before(result)'
check_order execute_effectful_operation_durable \
  'bodies->retain_application(request.application(), application)' \
  'journal.complete_application(application)'
check_order execute_effectful_operation_durable \
  'bodies->retain_publication_request(publication_request)' \
  'journal.begin_publication(transaction, publication_request)'
check_order execute_effectful_operation_durable \
  'bodies->retain_publication_receipt(' \
  'journal.complete_publication(publication_receipt)'

for forbidden in \
  'libpkgexec-linux' \
  'canonical_generation_store::initialize' \
  '.journal_namespace()' \
  '/var/lib/pkg'; do
  if grep -F -- "$forbidden" \
      "$model" "$model_source" "$codec_header" "$restart" \
      "$effect" "$store" "$codec" \
      >/dev/null 2>&1; then
    echo "forbidden durable-controller authority shortcut: $forbidden" >&2
    exit 1
  fi
done
