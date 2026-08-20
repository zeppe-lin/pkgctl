#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

pkgctl=$1
state_fixture=$2
state_inspect_fixture=$3
runtime_root_fixture=$4
fixture_collection=$5
root_view_fixture=$6
active_root=

cleanup()
{
  [ -n "$active_root" ] || return 0
  find "$active_root" -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$active_root"
  active_root=
}
trap cleanup EXIT HUP INT TERM

fail()
{
  printf 'pkgctl:cli-build-check-authority-refusal: %s\n' "$*" >&2
  exit 1
}

dump_file()
{
  label=$1
  file=$2
  printf '%s\n' "--- $label ---" >&2
  if [ -s "$file" ]; then cat "$file" >&2; else printf '%s\n' '<empty>' >&2; fi
}

run_scenario()
{
  scenario=$1
  root=$(mktemp -d "${TMPDIR:-/tmp}/pkgctl-check-authority-$scenario.XXXXXX")
  active_root=$root
  collection=$root/collection
  state=$root/state
  runtime=$root/runtime
  build=$root/build
  artifacts=$root/artifacts
  nonce=$(printf '%064d' "$2")
  cp -R "$fixture_collection" "$collection"
  binding=$("$state_fixture" "$state")
  uid=$(id -u)
  gid=$(id -g)
  groups=$(id -G | tr ' ' '\n' | sort -nu)

  mkdir "$runtime" "$build" "$artifacts"
  for directory in \
    command-evidence run evidence effects content construction-sessions \
    package-outputs check-resources check-temporary; do
    mkdir "$runtime/$directory"
  done
  "$root_view_fixture" "$build"
  chmod_program=$(command -v chmod) || fail 'host chmod is unavailable'
  interpreter=$("$runtime_root_fixture" "$build" /bin/sh "$chmod_program")

  initial_state=$("$state_inspect_fixture" "$state")
  set -- build archive-probe --check \
    --canonical-store "$state" \
    --collection "core=$collection" \
    --build-architecture x86_64 \
    --target-architecture x86_64 \
    --start "$nonce" \
    --build-parallelism 1 \
    --build-source-date-epoch 0 \
    --build-root-view "$(printf '%064d' 81)" \
    --runtime-root "$runtime" \
    --package-object-store "$root/package-objects" \
    --build-root "$build" \
    --artifact-root "$artifacts" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --max-steps 2
  for group in $groups; do
    [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"
  done

  set +e
  # shellcheck disable=SC2086
  "$pkgctl" "$@" $binding >"$root/run.out" 2>"$root/run.err"
  status=$?
  set -e
  if [ "$status" -ne 0 ]; then
    if grep -F 'native execution unavailable before transaction execution;' \
        "$root/run.err" >/dev/null; then
      dump_file 'native execution preflight' "$root/run.err"
      if [ "${PKGCTL_REQUIRE_NATIVE_INTEGRATION:-0}" = 1 ]; then
        fail 'release qualification requires native check authority refusal'
      fi
      exit 77
    fi
    dump_file 'initial stdout' "$root/run.out"
    dump_file 'initial stderr' "$root/run.err"
    fail "$scenario initial construction returned status $status"
  fi
  grep -Fx 'complete no' "$root/run.out" >/dev/null || \
    fail "$scenario did not stop before check"
  grep -Fx 'artifacts 2' "$root/run.out" >/dev/null || \
    fail "$scenario did not retain the two construction artifacts"

  case $scenario in
    source)
      digest=4fd7f5659897a904b772628cf3de2f03104cf284c45d305138c809639120d2e9
      prefix=$(printf '%s\n' "$digest" | cut -c1-2)
      authority=$runtime/content/sha256/$prefix/$digest
      [ -f "$authority" ] || fail 'retained archive source authority is absent'
      chmod u+w "$authority"
      printf 'mutated-after-construction\n' >>"$authority"
      chmod a-w "$authority"
      expected='native check source realization failed:'
      ;;
    package)
      probe_index=$(awk '
        $1 ~ /^artifact\.[0-9]+\.package$/ && $2 == "archive-probe" {
          split($1, fields, "."); print fields[2]; exit
        }
      ' "$root/run.out")
      [ -n "$probe_index" ] || fail 'archive-probe artifact index is absent'
      authority=$(awk -v key="artifact.$probe_index.path" \
        '$1 == key { print $2; exit }' "$root/run.out")
      [ -f "$authority" ] || fail 'retained archive-probe package authority is absent'
      chmod u+w "$authority"
      printf 'mutated-after-construction\n' >>"$authority"
      chmod a-w "$authority"
      expected='native check package realization failed:'
      ;;
    *)
      fail "unknown authority-refusal scenario: $scenario"
      ;;
  esac

  # Check must realize fresh resources from retained authority, not accidentally
  # consume construction-private trees that already happened to contain usable bytes.
  # Remember one exact planned leaf in each construction-private class, remove
  # all real residue, then recreate those exact leaves with hostile sentinels.
  # The upcoming check failure must authorize no terminal cleanup at all.
  construction_leaf=$(find "$runtime/construction-sessions" \
    -mindepth 2 -maxdepth 2 -type d | sort | sed -n '1p')
  package_leaf=$(find "$runtime/package-outputs" \
    -mindepth 2 -maxdepth 2 -type d | sort | sed -n '1p')
  [ -n "$construction_leaf" ] || fail 'construction cleanup leaf is absent'
  [ -n "$package_leaf" ] || fail 'package-output cleanup leaf is absent'
  construction_relative=${construction_leaf#"$runtime/construction-sessions/"}
  package_relative=${package_leaf#"$runtime/package-outputs/"}
  find "$runtime/construction-sessions" "$runtime/package-outputs" \
    -type d -exec chmod u+w {} + 2>/dev/null || :
  rm -rf "$runtime/construction-sessions" "$runtime/package-outputs"
  mkdir "$runtime/construction-sessions" "$runtime/package-outputs"
  construction_leaf=$runtime/construction-sessions/$construction_relative
  package_leaf=$runtime/package-outputs/$package_relative
  mkdir -p "$construction_leaf" "$package_leaf"
  printf '%s\n' 'must-survive-failed-check' >"$construction_leaf/sentinel"
  printf '%s\n' 'must-survive-failed-check' >"$package_leaf/sentinel"

  set -- build \
    --canonical-store "$state" \
    --resume "$nonce" \
    --runtime-root "$runtime" \
    --package-object-store "$root/package-objects" \
    --build-root "$build" \
    --artifact-root "$artifacts" \
    --interpreter "$interpreter" \
    --build-user-id "$uid" \
    --build-group-id "$gid" \
    --max-steps 1
  for group in $groups; do
    [ "$group" = "$gid" ] || set -- "$@" --build-supplementary-group "$group"
  done

  set +e
  "$pkgctl" "$@" >"$root/resume.out" 2>"$root/resume.err"
  status=$?
  set -e
  [ "$status" -ne 0 ] || {
    dump_file 'unexpected resume stdout' "$root/resume.out"
    dump_file 'unexpected resume stderr' "$root/resume.err"
    fail "$scenario mutated retained authority was admitted"
  }
  grep -F -- "$expected" "$root/resume.err" >/dev/null || {
    dump_file 'resume stdout' "$root/resume.out"
    dump_file 'resume stderr' "$root/resume.err"
    fail "$scenario failed outside the expected realization boundary"
  }

  if find "$runtime/check-temporary" -type f -name archive-check-ran -print -quit | \
      grep . >/dev/null; then
    fail "$scenario check program executed after retained authority corruption"
  fi
  [ "$(cat "$construction_leaf/sentinel")" = must-survive-failed-check ] || \
    fail "$scenario failed check cleaned construction-session residue"
  [ "$(cat "$package_leaf/sentinel")" = must-survive-failed-check ] || \
    fail "$scenario failed check cleaned package-output residue"
  if grep -F 'private realization cleanup' "$root/resume.err" >/dev/null; then
    fail "$scenario failed transaction attempted terminal cleanup"
  fi
  final_state=$("$state_inspect_fixture" "$state")
  [ "$initial_state" = "$final_state" ] || \
    fail "$scenario check authority failure changed canonical package state"

  cleanup
}

run_scenario source 31
run_scenario package 32
