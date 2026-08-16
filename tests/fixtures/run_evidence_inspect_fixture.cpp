// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/run_evidence_store.h>
#include <pkgctl/run_store.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string kind_name(pkgctl::transaction_unit_kind kind)
{
  switch (kind)
  {
    case pkgctl::transaction_unit_kind::construction:
      return "construction";
    case pkgctl::transaction_unit_kind::check:
      return "check";
    case pkgctl::transaction_unit_kind::operation:
      return "operation";
  }
  throw std::runtime_error("unknown dispatch kind");
}

std::string state_name(pkgctl::transaction_dispatch_state state)
{
  switch (state)
  {
    case pkgctl::transaction_dispatch_state::reserved:
      return "reserved";
    case pkgctl::transaction_dispatch_state::started:
      return "started";
    case pkgctl::transaction_dispatch_state::completed:
      return "completed";
    case pkgctl::transaction_dispatch_state::released_unstarted:
      return "released-unstarted";
  }
  throw std::runtime_error("unknown dispatch state");
}

const char* yes(bool value) noexcept
{
  return value ? "yes" : "no";
}

template<typename Evidence>
void require_evidence_binding(
    const pkgctl::transaction_run_journal_record& record,
    const pkgctl::transaction_dispatch_record& retained,
    const Evidence& evidence,
    const char* kind)
{
  const auto& dispatch = retained.dispatch();
  if (!retained.attempt_session() || !retained.terminal_evidence())
    throw std::runtime_error(std::string("completed ") + kind +
                             " lacks exact terminal authority");
  if (evidence.journal() != record.journal() ||
      evidence.transaction() != record.transaction() ||
      evidence.dispatch() != dispatch.identity() ||
      evidence.node() != dispatch.unit().primary_node() ||
      evidence.attempt_session() != *retained.attempt_session() ||
      evidence.result() != *retained.terminal_evidence())
  {
    throw std::runtime_error(std::string(kind) +
                             " evidence binding contradicts durable run head");
  }
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 4)
  {
    std::cerr << "usage: run-evidence-inspect-fixture RUN_STORE EVIDENCE_STORE "
                 "JOURNAL\n";
    return EXIT_FAILURE;
  }

  try
  {
    const auto journal = pkgctl::session_identity::from_hex(argv[3]);
    auto run_store = pkgctl::posix_transaction_run_journal_store::open(argv[1]);
    auto evidence_store =
        pkgctl::posix_transaction_run_evidence_store::open(argv[2]);
    const auto record = run_store.load_latest(journal);
    if (!record)
      throw std::runtime_error("selected journal has no durable head");

    std::cout << "record " << record->identity().hex() << '\n';
    std::cout << "sequence " << record->sequence() << '\n';
    std::cout << "complete " << yes(record->complete()) << '\n';
    std::cout << "failed " << yes(record->failed()) << '\n';
    std::cout << "stopped " << yes(record->stopped()) << '\n';
    std::cout << "dispatches " << record->dispatches().size() << '\n';

    std::size_t constructions = 0U;
    std::size_t checks = 0U;
    std::size_t construction_evidence = 0U;
    std::size_t check_evidence = 0U;
    for (std::size_t index = 0U; index < record->dispatches().size(); ++index)
    {
      const auto& retained = record->dispatches()[index];
      const auto& dispatch = retained.dispatch();
      std::cout << "dispatch." << index << ".identity "
                << dispatch.identity().hex() << '\n';
      std::cout << "dispatch." << index << ".kind "
                << kind_name(dispatch.unit().kind()) << '\n';
      std::cout << "dispatch." << index << ".state "
                << state_name(retained.state()) << '\n';
      std::cout << "dispatch." << index << ".attempt "
                << yes(retained.attempt_session().has_value()) << '\n';
      std::cout << "dispatch." << index << ".terminal-evidence "
                << yes(retained.terminal_evidence().has_value()) << '\n';

      if (retained.state() != pkgctl::transaction_dispatch_state::completed)
        continue;
      if (!retained.attempt_session() || !retained.terminal_evidence())
        throw std::runtime_error(
            "completed dispatch lacks exact terminal authority");

      if (dispatch.unit().kind() == pkgctl::transaction_unit_kind::construction)
      {
        ++constructions;
        const auto evidence = evidence_store.load_construction(
            record->journal(), dispatch.identity(),
            *retained.attempt_session());
        if (!evidence)
          throw std::runtime_error(
              "completed construction lacks retained evidence");
        require_evidence_binding(*record, retained, *evidence, "construction");
        ++construction_evidence;
        std::cout << "dispatch." << index << ".evidence-record "
                  << evidence->identity().hex() << '\n';
        std::cout << "dispatch." << index << ".result "
                  << evidence->result().hex() << '\n';
      }
      else if (dispatch.unit().kind() == pkgctl::transaction_unit_kind::check)
      {
        ++checks;
        const auto evidence = evidence_store.load_check(
            record->journal(), dispatch.identity(),
            *retained.attempt_session());
        if (!evidence)
          throw std::runtime_error("completed check lacks retained evidence");
        require_evidence_binding(*record, retained, *evidence, "check");
        ++check_evidence;
        std::cout << "dispatch." << index << ".evidence-record "
                  << evidence->identity().hex() << '\n';
        std::cout << "dispatch." << index << ".result "
                  << evidence->result().hex() << '\n';
      }
    }

    std::cout << "constructions " << constructions << '\n';
    std::cout << "checks " << checks << '\n';
    std::cout << "construction-evidence " << construction_evidence << '\n';
    std::cout << "check-evidence " << check_evidence << '\n';
  }
  catch (const std::exception& problem)
  {
    std::cerr << "run-evidence-inspect-fixture: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
