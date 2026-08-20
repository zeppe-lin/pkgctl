// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/construction_fixture.h"
#include "support/run_execute_support.h"

#include <libpkgobject/libpkgobject.h>

#include <pkgctl/run_admit.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_advance.h>
#include <pkgctl/run_drive.h>
#include <pkgctl/run_launch.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_runtime.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <unistd.h>

namespace {

using namespace construction_fixture;

int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << "CHECK failed: " #value "\n"; ++failures; } } while (false)

template<typename Function>
bool evidence_rejects(
    pkgctl::transaction_run_evidence_error_code expected,
    Function&& function)
{
  try
  {
    function();
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    return problem.code() == expected;
  }
  return false;
}

pkgctl::transaction_run_nonce journal_nonce(std::uint8_t marker)
{
  pkgctl::transaction_run_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_run_nonce::from_bytes(bytes);
}

pkgctl::transaction_dispatch_nonce dispatch_nonce(std::uint8_t marker)
{
  pkgctl::transaction_dispatch_nonce::byte_array bytes{};
  bytes.back() = marker;
  return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
}

std::filesystem::path evidence_file_with_suffix(
    const std::filesystem::path& directory,
    const std::string& suffix)
{
  std::optional<std::filesystem::path> selected;
  for (const auto& entry : std::filesystem::directory_iterator(directory))
  {
    const auto name = entry.path().filename().string();
    if (name.size() < suffix.size() ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
      continue;
    if (selected)
      throw std::runtime_error("evidence fixture found duplicate suffix");
    selected = entry.path();
  }
  if (!selected)
    throw std::runtime_error("evidence fixture did not find expected file");
  return *selected;
}

void flip_first_byte(const std::filesystem::path& path)
{
  const auto original_permissions = std::filesystem::status(path).permissions();
  std::filesystem::permissions(
      path, std::filesystem::perms::owner_write,
      std::filesystem::perm_options::add);
  try
  {
    std::fstream stream(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!stream)
      throw std::runtime_error("cannot open evidence fixture for corruption");
    char value = 0;
    stream.read(&value, 1);
    if (!stream)
      throw std::runtime_error("cannot read evidence fixture for corruption");
    value = static_cast<char>(static_cast<unsigned char>(value) ^ 0x01U);
    stream.seekp(0);
    stream.write(&value, 1);
    stream.flush();
    if (!stream)
      throw std::runtime_error("cannot corrupt evidence fixture");
  }
  catch (...)
  {
    std::filesystem::permissions(path, original_permissions);
    throw;
  }
  std::filesystem::permissions(path, original_permissions);
}


class fixed_progress_source final
    : public pkgctl::transaction_progress_rehydration_source {
public:
  explicit fixed_progress_source(pkgctl::transaction_progress progress)
      : progress_(std::move(progress))
  {
  }

  pkgctl::transaction_progress rehydrate_progress(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++calls_;
    requested_record_ = record.identity();
    return progress_;
  }

  void replace(pkgctl::transaction_progress progress)
  {
    progress_ = std::move(progress);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& requested_record() const noexcept
  { return requested_record_; }

private:
  pkgctl::transaction_progress progress_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> requested_record_;
};




class replay_run_nonce_source final
    : public pkgctl::transaction_run_nonce_source {
public:
  replay_run_nonce_source(
      std::uint8_t marker,
      std::vector<std::string>& trace,
      bool refuse = false)
      : marker_(marker), trace_(trace), refuse_(refuse)
  {
  }

  pkgctl::transaction_run_nonce issue(
      const pkgctl::transaction_run& run) override
  {
    ++calls_;
    runs_.push_back(run.identity());
    trace_.push_back("nonce");
    if (refuse_)
      throw std::runtime_error("injected run-nonce refusal");
    return journal_nonce(marker_);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::vector<pkgctl::session_identity>& runs() const noexcept
  { return runs_; }

private:
  std::uint8_t marker_;
  std::vector<std::string>& trace_;
  bool refuse_;
  std::size_t calls_ = 0U;
  std::vector<pkgctl::session_identity> runs_;
};

class admission_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit admission_run_store(
      std::vector<std::string>& trace,
      std::size_t fail_on_append = 0U,
      std::optional<pkgctl::transaction_run_journal_record> returned =
          std::nullopt)
      : trace_(trace), fail_on_append_(fail_on_append),
        returned_(std::move(returned))
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    if (!latest_ || latest_->journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++append_calls_;
    trace_.push_back("append");
    if (append_calls_ == fail_on_append_)
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected admission-store failure");
    if (latest_)
    {
      if (latest_->identity() != record.identity())
        throw pkgctl::transaction_run_journal_error(
            pkgctl::transaction_run_journal_error_code::store_conflict,
            "foreign sequence-zero admission");
    }
    else
    {
      if (record.sequence() != 0U || record.previous())
        throw std::runtime_error("invalid admission-store input");
      latest_ = record;
    }
    return returned_ ? *returned_ : *latest_;
  }

  std::size_t append_calls() const noexcept { return append_calls_; }
  const std::optional<pkgctl::transaction_run_journal_record>& latest()
      const noexcept
  { return latest_; }

private:
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  std::optional<pkgctl::transaction_run_journal_record> returned_;
  std::optional<pkgctl::transaction_run_journal_record> latest_;
  std::size_t append_calls_ = 0U;
};

class launch_run_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit launch_run_store(
      std::vector<std::string>& trace,
      std::size_t fail_on_append = 0U)
      : trace_(trace), fail_on_append_(fail_on_append)
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity& journal) const override
  {
    ++load_calls_;
    trace_.push_back("load");
    if (!latest_ || latest_->journal() != journal)
      return std::nullopt;
    return latest_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record& record) override
  {
    ++append_calls_;
    trace_.push_back("append-" + std::to_string(append_calls_));
    if (append_calls_ == fail_on_append_)
      throw pkgctl::transaction_run_journal_error(
          pkgctl::transaction_run_journal_error_code::store_write_failed,
          "injected launch-store failure");
    if (latest_ && latest_->identity() == record.identity())
      return *latest_;
    if (latest_)
      record.validate_successor_of(*latest_);
    else if (record.sequence() != 0U || record.previous())
      throw std::runtime_error("launch store did not begin at sequence zero");
    latest_ = record;
    return record;
  }

  std::size_t load_calls() const noexcept { return load_calls_; }
  std::size_t append_calls() const noexcept { return append_calls_; }
  const pkgctl::transaction_run_journal_record& latest() const
  {
    if (!latest_)
      throw std::runtime_error("launch store has no committed record");
    return *latest_;
  }

private:
  std::vector<std::string>& trace_;
  std::size_t fail_on_append_;
  mutable std::size_t load_calls_ = 0U;
  std::size_t append_calls_ = 0U;
  std::optional<pkgctl::transaction_run_journal_record> latest_;
};

class foreign_launch_head_store final
    : public pkgctl::transaction_run_journal_store {
public:
  explicit foreign_launch_head_store(
      pkgctl::transaction_run_journal_record record)
      : record_(std::move(record))
  {
  }

  std::optional<pkgctl::transaction_run_journal_record> load_latest(
      const pkgctl::session_identity&) const override
  {
    ++load_calls_;
    return record_;
  }

  pkgctl::transaction_run_journal_record append(
      const pkgctl::transaction_run_journal_record&) override
  {
    ++append_calls_;
    throw std::runtime_error("foreign launch store reached append");
  }

  std::size_t load_calls() const noexcept { return load_calls_; }
  std::size_t append_calls() const noexcept { return append_calls_; }

private:
  pkgctl::transaction_run_journal_record record_;
  mutable std::size_t load_calls_ = 0U;
  std::size_t append_calls_ = 0U;
};

class head_derived_nonce_source final
    : public pkgctl::transaction_dispatch_nonce_source {
public:
  explicit head_derived_nonce_source(std::uint8_t domain)
      : domain_(domain)
  {
  }

  pkgctl::transaction_dispatch_nonce issue(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_run& run) override
  {
    ++calls_;
    records_.push_back(record.identity());
    runs_.push_back(run.identity());
    pkgctl::transaction_dispatch_nonce::byte_array bytes{};
    bytes[0] = domain_;
    bytes[30] = static_cast<std::uint8_t>((record.sequence() >> 8U) & 0xffU);
    bytes[31] = static_cast<std::uint8_t>(record.sequence() & 0xffU);
    if (bytes[0] == 0U && bytes[30] == 0U && bytes[31] == 0U)
      bytes[0] = 1U;
    return pkgctl::transaction_dispatch_nonce::from_bytes(bytes);
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::vector<pkgctl::session_identity>& records() const noexcept
  { return records_; }
  const std::vector<pkgctl::session_identity>& runs() const noexcept
  { return runs_; }

private:
  std::uint8_t domain_;
  std::size_t calls_ = 0U;
  std::vector<pkgctl::session_identity> records_;
  std::vector<pkgctl::session_identity> runs_;
};

class construction_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source,
      public pkgctl::transaction_dispatch_session_source {
public:
  explicit construction_execution_authority_source(
      pkgctl::construction_session session)
      : session_(std::move(session))
  {
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_run& run,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = record.identity();
    run_ = run.identity();
    dispatch_ = dispatch.identity();
    return session_;
  }

  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record& record,
      const pkgctl::transaction_progress& progress,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = record.identity();
    run_ = progress.identity();
    dispatch_ = dispatch.identity();
    return session_;
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check execution authority request");
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check session authority request");
  }

  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation execution authority request");
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& run() const noexcept
  { return run_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::construction_session session_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> run_;
  std::optional<pkgctl::session_identity> dispatch_;
};


class throwing_construction_execution_authority_source final
    : public pkgctl::transaction_dispatch_execution_authority_source {
public:
  pkgctl::construction_session construction(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    ++calls_;
    throw std::runtime_error("injected execution-authority failure");
  }

  pkgctl::transaction_check_session check(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check execution authority request");
  }

  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation execution authority request");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class construction_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  explicit construction_recovery_authority_source(
      pkgctl::construction_result result)
      : authority_(std::move(result))
  {
  }

  explicit construction_recovery_authority_source(
      pkgctl::construction_session session)
      : authority_(std::move(session))
  {
  }

  pkgctl::construction_dispatch_recovery_authority construction(
      const pkgctl::transaction_run_restart_checkpoint& checkpoint,
      const pkgctl::transaction_dispatch_restart_assessment& assessment,
      const pkgctl::transaction_dispatch& dispatch) override
  {
    ++calls_;
    record_ = checkpoint.record().identity();
    assessment_ = assessment.dispatch();
    dispatch_ = dispatch.identity();
    return authority_;
  }

  pkgctl::check_dispatch_recovery_authority check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected check recovery authority request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery authority request");
  }

  std::size_t calls() const noexcept { return calls_; }
  const std::optional<pkgctl::session_identity>& record() const noexcept
  { return record_; }
  const std::optional<pkgctl::session_identity>& assessment() const noexcept
  { return assessment_; }
  const std::optional<pkgctl::session_identity>& dispatch() const noexcept
  { return dispatch_; }

private:
  pkgctl::construction_dispatch_recovery_authority authority_;
  std::size_t calls_ = 0U;
  std::optional<pkgctl::session_identity> record_;
  std::optional<pkgctl::session_identity> assessment_;
  std::optional<pkgctl::session_identity> dispatch_;
};


class construction_recovery_context_source final
    : public pkgctl::transaction_dispatch_recovery_context_source {
public:
  explicit construction_recovery_context_source(
      pkgctl::construction_result result)
      : result_(std::move(result))
  {
  }

  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    ++calls_;
    return {
        result_.session(), result_.materialization(),
        result_.build().execution().request(),
        result_.build().execution().backend(),
    };
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected check recovery context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery context request");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  pkgctl::construction_result result_;
  std::size_t calls_ = 0U;
};

class unreachable_recovery_context_source final
    : public pkgctl::transaction_dispatch_recovery_context_source {
public:
  pkgctl::construction_dispatch_recovery_context construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::construction_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected construction recovery context request");
  }

  pkgctl::check_dispatch_recovery_context check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&,
      const pkgctl::check_dispatch_evidence_record&) override
  {
    throw std::runtime_error("unexpected check recovery context request");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery context request");
  }
};

class unreachable_recovery_authority_source final
    : public pkgctl::transaction_dispatch_recovery_authority_source {
public:
  pkgctl::construction_dispatch_recovery_authority construction(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested construction evidence");
  }

  pkgctl::check_dispatch_recovery_authority check(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested check evidence");
  }

  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("reserved recovery requested operation evidence");
  }
};

class unreachable_operation_execution_authority_source final
    : public pkgctl::transaction_operation_execution_authority_source {
public:
  pkgctl::operation_dispatch_execution_authority operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_run&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation execution authority request");
  }
};

class unreachable_operation_recovery_context_source final
    : public pkgctl::transaction_operation_recovery_authority_source {
public:
  pkgctl::effect_restart_checkpoint operation(
      const pkgctl::transaction_run_restart_checkpoint&,
      const pkgctl::transaction_dispatch_restart_assessment&,
      const pkgctl::transaction_dispatch&) override
  {
    throw std::runtime_error("unexpected operation recovery authority request");
  }
};

class forbidden_runtime_archive_source final
    : public pkgctl::transaction_effect_archive_source {
public:
  std::unique_ptr<pkgimage::package_archive> open_archive(
      const pkgapply::incoming_package_authority&) override
  {
    ++calls_;
    throw std::runtime_error("construction runtime requested an archive");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class refusing_native_installed_package_source final
    : public pkgctl::retained_installed_package_tree_source {
public:
  pkgctl::retained_installed_package_tree locate(
      const pkgstate::installed_package&) override
  {
    ++calls_;
    throw std::runtime_error(
        "native construction runtime requested an installed package tree");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class unreachable_native_operation_specification_source final
    : public pkgctl::transaction_operation_specification_source {
public:
  pkgctl::native_transaction_operation_specification operation(
      const pkgctl::transaction_run_journal_record&,
      const pkgctl::transaction_progress&,
      const pkgctl::transaction_dispatch&) override
  {
    ++calls_;
    throw std::runtime_error(
        "native construction runtime requested operation sensing");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

class unreachable_native_effect_restart_body_source final
    : public pkgctl::transaction_effect_restart_body_source {
public:
  pkgctl::transaction_effect_restart_bodies load(
      const pkgctl::effectful_operation_session&,
      const pkgctl::effect_attempt_record&) override
  {
    ++calls_;
    throw std::runtime_error(
        "native construction runtime requested effect restart bodies");
  }

  std::size_t calls() const noexcept { return calls_; }

private:
  std::size_t calls_ = 0U;
};

pkgapply_exec::lifecycle_execution_identity
native_runtime_lifecycle_execution_identity()
{
  return {
      pkgexec::interpreter_identity::from_sha256(std::string(64U, '6')),
      static_cast<std::uint64_t>(::geteuid()),
      static_cast<std::uint64_t>(::getegid()),
      {},
  };
}

pkgctl::native_transaction_session_configuration
native_runtime_session_configuration(const std::filesystem::path& root)
{
  const auto root_view = root / "execution-root";
  std::filesystem::create_directories(root_view);
  return pkgctl::native_transaction_session_configuration::make(
      {
          root / "content",
          root / "construction-sessions",
          root / "package-outputs",
          root / "artifacts",
          root / "installed-resources",
          root / "check-resources",
          root / "check-temporary",
          pkgexec::root_view_identity::from_sha256(std::string(64U, '5')),
          root_view,
      },
      {
          pkgbuild::build_policy::make(
              pkgbuild::environment_policy::hermetic(
                  2, 0022, 1700000000)),
          pkgfetch::acquisition_policy::defaults(),
          {
              pkgexec::interpreter_identity::from_sha256(
                  std::string(64U, '4')),
              static_cast<std::uint64_t>(::geteuid()),
              static_cast<std::uint64_t>(::getegid()),
              {},
          },
          {
              pkgexec::interpreter_identity::from_sha256(
                  std::string(64U, '3')),
              static_cast<std::uint64_t>(::geteuid()),
              static_cast<std::uint64_t>(::getegid()),
              {},
          },
          pkgexec::resource_limits::make(),
          pkgbuild::artifact_compression::none,
      });
}

pkgctl::native_transaction_operation_configuration
native_runtime_operation_configuration(
    const pkgctl::transaction_session& transaction,
    const std::filesystem::path& root)
{
  const auto execution_root = root / "lifecycle-execution-root";
  std::filesystem::create_directories(execution_root);
  return pkgctl::native_transaction_operation_configuration::make(
      transaction, package_policy(),
      {
          pkgexec::root_view_identity::from_sha256(std::string(64U, '7')),
          execution_root,
          root / "target",
          root / "lifecycle-sessions",
          native_runtime_lifecycle_execution_identity(),
      });
}

class unreachable_runtime_application_backend final
    : public pkgapply::application_backend {
public:
  unreachable_runtime_application_backend()
      : mutation_(apply_identity<pkgapply::mutation_backend_identity>(201U)),
        observation_(
            apply_identity<pkgapply::observation_backend_identity>(202U)),
        capabilities_(
            apply_identity<pkgapply::execution_capability_profile_identity>(
                203U))
  {
  }

  const pkgapply::mutation_backend_identity& identity() const noexcept override
  {
    return mutation_;
  }

  const pkgapply::observation_backend_identity&
  observation_identity() const noexcept override
  {
    return observation_;
  }

  const pkgapply::execution_capability_profile_identity&
  capabilities() const noexcept override
  {
    return capabilities_;
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  begin_with_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&,
      const pkgimage::package_image&) override
  {
    throw std::runtime_error(
        "construction runtime reached incoming application authority");
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  begin_without_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&) override
  {
    throw std::runtime_error(
        "construction runtime reached removal application authority");
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  resume_with_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&,
      const pkgapply::application_restart_view&,
      const pkgimage::package_image&) override
  {
    throw std::runtime_error(
        "construction runtime reached incoming application restart");
  }

  std::unique_ptr<pkgapply::application_backend_transaction>
  resume_without_incoming_image(
      const pkgapply::package_application_request&,
      pkgapply::target_mutation_lease&,
      const pkgapply::application_restart_view&) override
  {
    throw std::runtime_error(
        "construction runtime reached removal application restart");
  }

private:
  pkgapply::mutation_backend_identity mutation_;
  pkgapply::observation_backend_identity observation_;
  pkgapply::execution_capability_profile_identity capabilities_;
};

class unreachable_runtime_application_journal_store final
    : public pkgapply::application_journal_store {
public:
  pkgapply::application_journal_declaration publish_declaration(
      const pkgapply::application_journal_declaration&) override
  { throw std::runtime_error("construction runtime reached journal publication"); }
  pkgapply::application_journal_step publish_step(
      const pkgapply::application_journal_step&) override
  { throw std::runtime_error("construction runtime reached journal publication"); }
  pkgapply::application_journal_cursor compare_and_publish_cursor(
      const std::optional<pkgapply::application_journal_cursor_identity>&,
      const pkgapply::application_journal_cursor&) override
  { throw std::runtime_error("construction runtime reached cursor publication"); }
  std::optional<pkgapply::application_journal_declaration> load_declaration(
      const pkgapply::application_journal_declaration_identity&) override
  { throw std::runtime_error("construction runtime reached journal load"); }
  std::optional<pkgapply::application_journal_cursor> load_cursor(
      const pkgapply::application_journal_declaration_identity&) override
  { throw std::runtime_error("construction runtime reached cursor load"); }
  std::optional<pkgapply::application_journal_step> load_step(
      const pkgapply::application_journal_declaration_identity&,
      std::uint64_t) override
  { throw std::runtime_error("construction runtime reached journal load"); }
};

class forbidden_recovery_execution_backend final
    : public pkgexec::execution_backend {
public:
  pkgexec::backend_capability_profile capabilities() const override
  {
    ++capability_calls_;
    throw std::runtime_error(
        "durable recovery queried current execution capabilities");
  }

  pkgexec::execution_result execute(
      const pkgexec::execution_request&,
      const pkgexec::execution_resources&) override
  {
    ++execution_calls_;
    throw std::runtime_error("durable recovery reached current execution");
  }

  std::size_t capability_calls() const noexcept { return capability_calls_; }
  std::size_t execution_calls() const noexcept { return execution_calls_; }

private:
  mutable std::size_t capability_calls_ = 0U;
  std::size_t execution_calls_ = 0U;
};

int open_runtime_directory(const std::filesystem::path& path)
{
  const int fd = ::open(
      path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    throw std::runtime_error(
        "cannot open runtime test directory: " +
        std::string(std::strerror(errno)));
  return fd;
}

std::size_t directory_entry_count(const std::filesystem::path& path)
{
  return static_cast<std::size_t>(
      std::distance(std::filesystem::directory_iterator(path),
                    std::filesystem::directory_iterator()));
}

class throwing_construction_driver final : public pkgctl::construction_driver {
public:
  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request&) override
  {
    throw std::runtime_error("driver escaped without materialization evidence");
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session&) override
  {
    throw std::runtime_error("unreachable build execution");
  }

  void publish_build(
      const pkgbuild_exec::admitted_build_session&,
      const pkgbuild_exec::build_execution_result&) override
  {
    throw std::runtime_error("unreachable artifact publication");
  }
};

class tracing_construction_driver final : public pkgctl::construction_driver {
public:
  tracing_construction_driver(
      pkgctl::construction_driver& driver, std::vector<std::string>& trace)
      : driver_(driver), trace_(trace)
  {
  }

  pkgfetch::source_materialization materialize_source(
      const pkgfetch::materialization_request& request) override
  {
    trace_.push_back("materialize");
    return driver_.materialize_source(request);
  }

  pkgbuild_exec::build_execution_result execute_build(
      const pkgbuild_exec::admitted_build_session& session) override
  {
    trace_.push_back("build");
    return driver_.execute_build(session);
  }

  void publish_build(
      const pkgbuild_exec::admitted_build_session& session,
      const pkgbuild_exec::build_execution_result& result) override
  {
    trace_.push_back("publish");
    driver_.publish_build(session, result);
  }

private:
  pkgctl::construction_driver& driver_;
  std::vector<std::string>& trace_;
};

void check_success()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto session = construction_session(transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  CHECK(result.succeeded());
  CHECK(result.outcome() == pkgctl::construction_outcome::completed);
  CHECK(result.materialization().source().identity() == source.identity());
  CHECK(result.materialization().objects().size() == 1U);
  CHECK(result.build().build().outcome() == pkgbuild::build_outcome::succeeded);
  CHECK(result.build().image_authority().has_value());
  CHECK(fs::is_regular_file(session.paths().build.artifact_path));

  auto progression = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::pending);
  bool refused = false;
  try
  {
    (void)pkgctl::advance_construction(progression, result);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(refused);

  auto other_transaction = transaction_session(
      tool_source(sha256_text(payload), "2.0"), dependency_source(),
      store.read(), temporary.path() / "state");
  auto other_progression =
      pkgctl::transaction_progress::begin(other_transaction);
  refused = false;
  try
  {
    (void)pkgctl::advance_construction(other_progression, result);
  }
  catch (const pkgctl::error& problem)
  {
    refused = problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(refused);
}

void check_install_preparation()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true, true);
  CHECK(!transaction.program().nodes_for(
      pkgsource::package_reference("dep")).empty());
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver construction_driver(backend);
  auto construction = pkgctl::execute_construction(session, construction_driver);

  const auto target_system =
      plan_identity<pkgplan::target_system_context_identity>(52);
  auto progression = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  bool dependency_ready = false;
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::build &&
        node.package().name() == "dep")
    {
      dependency_ready =
          progression.status(node.identity()) ==
          pkgctl::transaction_node_status::ready;
    }
  }
  CHECK(dependency_ready);
  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.status(install_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);

  auto request = pkgctl::operation_preparation_request::install(
      progression, install_node(transaction).identity(), construction,
      application_target(store.read().target_binding(), target_system),
      execution_control(), empty_target_observations(target_system),
      plan_identity<pkgplan::runtime_dependency_closure_identity>(53),
      package_policy(), pkgctl::lifecycle_order::make({}, {}),
      pkgstate::installation_reason::explicit_request());
  pkgimage::libarchive_backend archives;
  pkgctl::native_operation_preparation_driver preparation_driver;
  const auto result = pkgctl::prepare_operation(
      std::move(request), preparation_driver);

  CHECK(result.prepared());
  CHECK(result.artifact().has_value());
  CHECK(result.incoming().has_value());
  CHECK(!result.refusal());
  CHECK(result.plan() &&
        result.plan()->kind() == pkgplan::operation_kind::install);
  CHECK(result.application() && result.application()->installation());
  CHECK(result.application() && result.plan() &&
        result.application()->plan() == result.plan()->identity());
  CHECK(result.application() && result.incoming() &&
        result.application()->incoming() &&
        result.application()->incoming()->identity() ==
            result.incoming()->identity());
  CHECK(result.effect() &&
        result.effect()->action_node() == install_node(transaction).identity());
  CHECK(result.effect() &&
        result.effect()->transaction().identity() == transaction.identity());
  CHECK(result.effect() && result.application() &&
        result.effect()->application().identity() ==
            result.application()->identity());
  CHECK(result.effect() &&
        result.effect()->expected_state().identity() ==
            progression.current_state().identity());
  CHECK(result.artifact() && construction.build().image_authority() &&
        result.artifact()->authority().image().receipt().archive_digest() ==
            construction.build().image_authority()->image().receipt().archive_digest());
  CHECK(result.artifact()->authority().image().receipt().image_identity() ==
        construction.build().image_authority()->image().receipt().image_identity());
  CHECK(result.artifact()->authority().image().receipt().entry_count() ==
        construction.build().image_authority()->image().receipt().entry_count());
}

void check_check_progression()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(
      sha256_text(payload), "1.0", false,
      pkgsource::program(pkgsource::program_language::posix_shell, "true\n"));
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      false, false, true);
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto construction = pkgctl::execute_construction(session, driver);

  auto progression = pkgctl::transaction_progress::begin(transaction);
  const auto repeated = pkgctl::transaction_progress::begin(transaction);
  CHECK(progression.identity() == repeated.identity());
  const auto initial_progress = progression.identity();
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  CHECK(check_node(transaction).check_program());
  CHECK(check_node(transaction).check_program()->material() == "true\n");
  CHECK(progression.status(check_node(transaction).identity()) ==
        pkgctl::transaction_node_status::pending);
  CHECK(progression.ready_units().size() == 1U);
  CHECK(progression.ready_units().front().kind() ==
        pkgctl::transaction_unit_kind::construction);

  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.identity() != initial_progress);
  CHECK(progression.status(check_node(transaction).identity()) ==
        pkgctl::transaction_node_status::ready);
  bool duplicate_refused = false;
  try
  {
    (void)pkgctl::advance_construction(progression, construction);
  }
  catch (const pkgctl::error& problem)
  {
    duplicate_refused =
        problem.code() == pkgctl::error_code::invalid_progression;
  }
  CHECK(duplicate_refused);
  CHECK(progression.ready_units().size() == 1U);
  CHECK(progression.ready_units().front().kind() ==
        pkgctl::transaction_unit_kind::check);
  CHECK(progression.ready_units().front().primary_node() ==
        check_node(transaction).identity());
  CHECK(!progression.complete());
  CHECK(!progression.failed());
}

void check_failed_progression()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state",
      true);
  auto session = construction_session_without_inputs(
      transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::fail);
  pkgctl::native_construction_driver driver(backend);
  auto construction = pkgctl::execute_construction(session, driver);
  CHECK(!construction.succeeded());

  auto progression = pkgctl::transaction_progress::begin(transaction);
  const auto initial_state = progression.current_state().identity();
  progression = pkgctl::advance_construction(
      std::move(progression), construction);
  CHECK(progression.status(build_node(transaction).identity()) ==
        pkgctl::transaction_node_status::failed);
  CHECK(progression.status(install_node(transaction).identity()) ==
        pkgctl::transaction_node_status::blocked);
  CHECK(progression.current_state().identity() == initial_state);
  CHECK(progression.failed());
  CHECK(!progression.complete());
  CHECK(progression.ready_units().empty());
}

void check_failed_build()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto session = construction_session(transaction, temporary.path());
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::fail);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);
  CHECK(!result.succeeded());
  CHECK(result.outcome() == pkgctl::construction_outcome::build_failed);
  CHECK(result.build().build().outcome() == pkgbuild::build_outcome::failed);
  CHECK(!result.build().image_authority().has_value());
  CHECK(!fs::exists(session.paths().build.artifact_path));
}

void check_transitive_build_only_check_authority()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());

  tool_source_options options;
  options.check_program = pkgsource::program(
      pkgsource::program_language::posix_shell, "true\n");
  const auto source = tool_source(sha256_text("root payload\n"),
                                  std::move(options));
  const auto dependency = package_source_with_requirements(
      "dep", {"helper"}, {"helper"});
  const auto helper = package_source("helper");
  auto transaction = transaction_session(
      source, {dependency, helper}, store.read(), temporary.path() / "state",
      false, false, true);

  bool dependency_check = false;
  for (const auto& node : transaction.program().nodes())
  {
    if (node.action() == pkgtransaction::transaction_action_kind::check &&
        node.package().name() == "dep")
      dependency_check = true;
  }
  CHECK(!dependency_check);

  const auto request = pkgctl::construction_request::make(
      transaction, build_node(transaction, "dep").identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1, 0022, std::nullopt)));
  CHECK(request.inputs().size() == 1U);
  CHECK(request.inputs().front().scope() == pkgbuild::input_scope::build);
  CHECK(request.inputs().front().package().name() == "helper");
  CHECK(request.build().inputs().for_scope(pkgbuild::input_scope::check).empty());
}

void check_admission()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");

  bool unknown_node = false;
  try
  {
    (void)pkgctl::construction_request::make(
        transaction,
        source_identity<pkgtransaction::transaction_node_identity>('f'),
        pkgbuild::build_policy::make(
            pkgbuild::environment_policy::hermetic(1, 0022, std::nullopt)));
  }
  catch (const pkgctl::error& value)
  {
    unknown_node = value.code() ==
        pkgctl::error_code::invalid_construction_request;
  }
  CHECK(unknown_node);

  auto request = pkgctl::construction_request::make(
      transaction, build_node(transaction).identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(1, 0022, std::nullopt)));
  CHECK(request.inputs().size() == 1U);
  const auto& input = request.inputs().front();
  pkgbuild_exec::package_input_resource exact{
      input.identity(),
      pkgexec::resource_identity::from_sha256(input.identity().hex()),
      temporary.path() / "exact",
  };
  pkgbuild_exec::package_input_resource extra{
      source_identity<pkgbuild::build_input_identity>('1'),
      source_identity<pkgexec::resource_identity>('2'),
      temporary.path() / "extra",
  };
  bool extra_resource = false;
  try
  {
    (void)pkgctl::construction_session::admit(
        request,
        {temporary.path() / "sources", temporary.path() / "store",
         {pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          temporary.path() / "root", temporary.path() / "session",
          temporary.path() / "package", temporary.path() / "artifact.tar"}},
        {exact, extra},
        {pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
         static_cast<std::uint64_t>(::geteuid()),
         static_cast<std::uint64_t>(::getegid()), {}});
  }
  catch (const pkgctl::error& value)
  {
    extra_resource = value.code() ==
        pkgctl::error_code::invalid_construction_session;
  }
  CHECK(extra_resource);

  bool missing_resource = false;
  try
  {
    (void)pkgctl::construction_session::admit(
        request,
        {temporary.path() / "sources", temporary.path() / "store",
         {pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          temporary.path() / "root", temporary.path() / "session",
          temporary.path() / "package", temporary.path() / "artifact.tar"}},
        {},
        {pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
         static_cast<std::uint64_t>(::geteuid()),
         static_cast<std::uint64_t>(::getegid()), {}});
  }
  catch (const pkgctl::error& value)
  {
    missing_resource = value.code() ==
        pkgctl::error_code::invalid_construction_session;
  }
  CHECK(missing_resource);

  bool overlapping_coordinates = false;
  try
  {
    (void)pkgctl::construction_session::admit(
        request,
        {temporary.path() / "shared", temporary.path() / "shared" / "store",
         {pkgexec::root_view_identity::from_sha256(std::string(64U, 'b')),
          temporary.path() / "root", temporary.path() / "session",
          temporary.path() / "package", temporary.path() / "artifact.tar"}},
        {exact},
        {pkgexec::interpreter_identity::from_sha256(std::string(64U, 'c')),
         static_cast<std::uint64_t>(::geteuid()),
         static_cast<std::uint64_t>(::getegid()), {}});
  }
  catch (const pkgctl::error& value)
  {
    overlapping_coordinates = value.code() ==
        pkgctl::error_code::invalid_construction_session;
  }
  CHECK(overlapping_coordinates);
}


void check_identity_and_driver_contract()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "source payload\n";
  auto source = tool_source(sha256_text(payload));
  auto transaction = transaction_session(source, dependency_source(), store.read(),
                                         temporary.path() / "state");
  auto first = construction_session(transaction, temporary.path() / "first");
  auto second = construction_session(transaction, temporary.path() / "second");
  CHECK(first.identity() != second.identity());

  test_support::write(first.paths().local_source_root / "payload", payload);
  mismatched_materialization_driver mismatch(
      tool_source(sha256_text(payload), "2.0"));
  bool rejected = false;
  try
  {
    (void)pkgctl::execute_construction(first, mismatch);
  }
  catch (const pkgctl::error& value)
  {
    rejected = value.code() ==
        pkgctl::error_code::construction_driver_contract_violation;
  }
  CHECK(rejected);

  auto build_session = construction_session(
      transaction, temporary.path() / "mismatched-build");
  test_support::write(
      build_session.paths().local_source_root / "payload", payload);
  const auto& subject = construction_subject_selection(transaction);
  auto alternate = pkgbuild::build_request::seal(
      transaction.resolution().resolution(), subject.identity(),
      pkgbuild::build_policy::make(
          pkgbuild::environment_policy::hermetic(3, 0022, 1700000000)));
  mismatched_build_driver mismatched_build(std::move(alternate));
  bool build_rejected = false;
  try
  {
    (void)pkgctl::execute_construction(build_session, mismatched_build);
  }
  catch (const pkgctl::error& value)
  {
    build_rejected = value.code() ==
        pkgctl::error_code::construction_driver_contract_violation;
  }
  CHECK(build_rejected);
}


void check_durable_dispatch_execution()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction, std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::construction)
      throw std::runtime_error(
          "fixture did not reserve a construction dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  const auto make_fixture = [](const std::filesystem::path& root) {
    test_support::initialize_state(root / "state");
    pkgstate::posix::canonical_generation_store store(
        root / "state", test_support::binding());
    const std::string payload = "durable construction payload\n";
    auto source = tool_source(sha256_text(payload), "1.0", false);
    auto transaction = transaction_session(
        source, dependency_source(), store.read(), root / "state");
    auto session = construction_session_without_inputs(
        transaction, root / "construction");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    return std::make_pair(std::move(transaction), std::move(session));
  };

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 41U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    const auto completed = pkgctl::execute_construction_dispatch_durable(
        reserved, reservation.run, *reservation.dispatch,
        fixture.second, driver, evidence_store, run_store);

    CHECK(trace == std::vector<std::string>(
        {"attempt-construction", "run-1", "materialize", "build",
         "evidence-construction", "publish", "run-2"}));
    CHECK(completed.result.succeeded());
    CHECK(completed.evidence.result() == completed.result.identity());
    CHECK(completed.record.sequence() == reserved.sequence() + 2U);
    CHECK(completed.run.records().size() == 1U);
    if (completed.run.records().size() == 1U)
    {
      CHECK(completed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
      CHECK(completed.run.records().front().terminal_evidence() ==
            std::optional<pkgctl::session_identity>(
                completed.result.identity()));
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 40U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(
        trace, false, false, true, false);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"attempt-construction"}));
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 42U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 1U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-construction", "run-1"}));
    CHECK(run_store.latest().identity() == reserved.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 43U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 2U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-construction", "run-1", "materialize", "build",
         "evidence-construction", "publish", "run-2"}));
    CHECK(fs::is_regular_file(fixture.second.paths().build.artifact_path));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto reopened = run_store.latest().reopen(reservation.run.progress());
    CHECK(reopened.records().size() == 1U);
    if (reopened.records().size() == 1U)
    {
      CHECK(reopened.records().front().state() ==
            pkgctl::transaction_dispatch_state::started);
    }
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                recover_construction);
    }
    CHECK(!run_store.latest().dispatches().empty());
    if (!run_store.latest().dispatches().empty() &&
        run_store.latest().dispatches().front().attempt_session())
    {
      const auto retained = evidence_store.load_construction(
          run_store.latest().journal(), reservation.dispatch->identity(),
          *run_store.latest().dispatches().front().attempt_session());
      CHECK(retained.has_value());
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 45U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(
        trace, true, false);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver native_driver(backend);
    tracing_construction_driver driver(native_driver, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, evidence_store, run_store);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-construction", "run-1", "materialize", "build",
         "evidence-construction"}));
    CHECK(!fs::exists(fixture.second.paths().build.artifact_path));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                recover_construction);
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 44U);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace, 0U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    throwing_construction_driver escaped;
    tracing_construction_driver driver(escaped, trace);
    bool failed = false;
    try
    {
      (void)pkgctl::execute_construction_dispatch_durable(
          reserved, reservation.run, *reservation.dispatch,
          fixture.second, driver, evidence_store, run_store);
    }
    catch (const std::runtime_error&)
    {
      failed = true;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>(
        {"attempt-construction", "run-1", "materialize"}));
    CHECK(run_store.latest().sequence() == reserved.sequence() + 1U);
    const auto assessment = pkgctl::transaction_run_restart_checkpoint::make(
        reservation.run.progress(), run_store.latest()).assessment();
    CHECK(assessment.active().size() == 1U);
    if (assessment.active().size() == 1U)
    {
      CHECK(assessment.active().front().disposition() ==
            pkgctl::transaction_dispatch_restart_disposition::
                recover_construction);
    }
  }
}



void check_transaction_run_evidence_storage()
{
  test_support::temporary_directory temporary;
  const auto state_path = temporary.path() / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "durable evidence payload\n";
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), "1.0", false), dependency_source(),
      state_store.read(), state_path);
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  auto run = pkgctl::transaction_run::begin(
      pkgctl::transaction_progress::begin(transaction),
      pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(61U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(61U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    throw std::runtime_error("evidence fixture did not reserve construction");
  auto reserved = admitted.successor(reservation.run);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  const auto result = pkgctl::execute_construction(session, driver);
  const auto record = pkgctl::construction_dispatch_evidence_record::admit(
      started, *reservation.dispatch, result);

  CHECK(record.journal() == started.journal());
  CHECK(record.transaction() == transaction.identity());
  CHECK(record.dispatch() == reservation.dispatch->identity());
  CHECK(record.node() == reservation.dispatch->unit().primary_node());
  CHECK(record.attempt_session() == session.identity());
  CHECK(record.result() == result.identity());
  CHECK(record.controller_request() == session.request().identity());
  CHECK(record.session_encoding() == pkgctl::encode_construction_session(session));
  CHECK(record.materialization() == result.materialization().identity());
  CHECK(record.materialization_encoding() ==
        pkgfetch::encode_source_materialization(result.materialization()));
  CHECK(record.build_request() == session.request().build().identity());
  CHECK(record.execution_request() ==
        result.build().execution().request().identity());
  CHECK(record.backend() == result.build().execution().backend().identity());
  CHECK(record.backend_encoding() == pkgexec::encode_backend_capability_profile(
        result.build().execution().backend()));
  CHECK(record.execution() == result.build().execution().identity());
  CHECK(record.build() == result.build().build().identity());

  const auto encoding =
      pkgctl::encode_construction_dispatch_evidence(record);
  const auto decoded =
      pkgctl::decode_construction_dispatch_evidence(encoding);
  CHECK(decoded.identity() == record.identity());
  CHECK(decoded.session_encoding() == record.session_encoding());
  const auto decoded_session = pkgctl::decode_construction_session(
      decoded.session_encoding(), transaction,
      reservation.dispatch->unit().primary_node());
  CHECK(decoded_session.identity() == session.identity());
  CHECK(decoded_session.paths().local_source_root ==
        session.paths().local_source_root);
  CHECK(decoded_session.paths().content_store_root ==
        session.paths().content_store_root);
  CHECK(decoded_session.execution_identity().interpreter ==
        session.execution_identity().interpreter);
  CHECK(decoded.materialization_encoding() ==
        record.materialization_encoding());
  CHECK(decoded.backend_encoding() == record.backend_encoding());
  CHECK(pkgexec::decode_backend_capability_profile(decoded.backend_encoding()) ==
        result.build().execution().backend());
  CHECK(decoded.encoding() == record.encoding());
  CHECK(pkgctl::encode_construction_dispatch_evidence(decoded) == encoding);

  {
    auto corrupt_session = decoded.session_encoding();
    corrupt_session.back() ^= 0x01U;
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_construction_session(
          corrupt_session, transaction,
          reservation.dispatch->unit().primary_node());
    }
    catch (const pkgctl::construction_codec_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::construction_codec_error_code::corrupt_encoding;
    }
    CHECK(rejected);
  }

  {
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_construction_session(
          decoded.session_encoding(), transaction,
          pkgtransaction::transaction_node_identity::from_sha256(
              std::string(64U, 'f')));
    }
    catch (const pkgctl::construction_codec_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::construction_codec_error_code::authority_mismatch;
    }
    CHECK(rejected);
  }

  {
    auto corrupt = encoding;
    corrupt.front() ^= 0x01U;
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_construction_dispatch_evidence(corrupt);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::corrupt_encoding;
    }
    CHECK(rejected);
  }

  {
    auto truncated = encoding;
    truncated.pop_back();
    bool rejected = false;
    try
    {
      (void)pkgctl::decode_construction_dispatch_evidence(truncated);
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::corrupt_encoding;
    }
    CHECK(rejected);
  }

  const auto original_path = temporary.path() / "evidence-original";
  const auto selected_path = temporary.path() / "evidence-selected";
  std::filesystem::create_directory(original_path);
  const int directory_fd = open_runtime_directory(original_path);
  auto store = pkgctl::posix_transaction_run_evidence_store::from_directory_fd(
      directory_fd);
  CHECK(::close(directory_fd) == 0);
  std::filesystem::rename(original_path, selected_path);
  std::filesystem::create_directory(original_path);

  CHECK(!store.load_construction(
      record.journal(), record.dispatch(),
      pkgctl::session_identity::from_hex(std::string(64U, 'f'))));
  CHECK(!std::filesystem::exists(
      selected_path / ".pkgctl-run-evidence.lock"));
  CHECK(store.publish(record).identity() == record.identity());
  CHECK(store.publish(record).identity() == record.identity());
  const auto loaded = store.load_construction(
      record.journal(), record.dispatch(), record.attempt_session());
  CHECK(loaded.has_value());
  CHECK(loaded && loaded->identity() == record.identity());
  CHECK(directory_entry_count(selected_path) >= 3U);
  CHECK(directory_entry_count(original_path) == 0U);
  const auto object_permissions = std::filesystem::status(
      evidence_file_with_suffix(selected_path, ".pce")).permissions();
  const auto index_permissions = std::filesystem::status(
      evidence_file_with_suffix(selected_path, ".pci")).permissions();
  CHECK((object_permissions & std::filesystem::perms::owner_write) ==
        std::filesystem::perms::none);
  CHECK((index_permissions & std::filesystem::perms::owner_write) ==
        std::filesystem::perms::none);

  auto reopened = pkgctl::posix_transaction_run_evidence_store::open(
      selected_path.string());
  const auto reopened_record = reopened.load_construction(
      record.journal(), record.dispatch(), record.attempt_session());
  CHECK(reopened_record.has_value());
  CHECK(reopened_record && reopened_record->identity() == record.identity());

  fixture_backend failing_backend(backend_mode::fail);
  pkgctl::native_construction_driver failing_driver(failing_backend);
  const auto alternate_result =
      pkgctl::execute_construction(session, failing_driver);
  const auto alternate = pkgctl::construction_dispatch_evidence_record::admit(
      started, *reservation.dispatch, alternate_result);
  CHECK(alternate.identity() != record.identity());
  bool conflict = false;
  try
  {
    (void)store.publish(alternate);
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    conflict = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::store_conflict;
  }
  CHECK(conflict);

  {
    const auto absent_path = temporary.path() / "evidence-absent-object";
    std::filesystem::create_directory(absent_path);
    auto absent_store = pkgctl::posix_transaction_run_evidence_store::open(
        absent_path.string());
    (void)absent_store.publish(record);
    std::filesystem::remove(evidence_file_with_suffix(absent_path, ".pce"));
    bool rejected = false;
    try
    {
      (void)absent_store.load_construction(
          record.journal(), record.dispatch(), record.attempt_session());
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_corrupt;
    }
    CHECK(rejected);
  }

  {
    const auto corrupt_index_path = temporary.path() / "evidence-corrupt-index";
    std::filesystem::create_directory(corrupt_index_path);
    auto corrupt_store = pkgctl::posix_transaction_run_evidence_store::open(
        corrupt_index_path.string());
    (void)corrupt_store.publish(record);
    flip_first_byte(evidence_file_with_suffix(corrupt_index_path, ".pci"));
    bool rejected = false;
    try
    {
      (void)corrupt_store.load_construction(
          record.journal(), record.dispatch(), record.attempt_session());
    }
    catch (const pkgctl::transaction_run_evidence_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_evidence_error_code::store_corrupt;
    }
    CHECK(rejected);
  }


  {
    const auto fifo_index_path = temporary.path() / "evidence-fifo-index";
    std::filesystem::create_directory(fifo_index_path);
    auto fifo_store = pkgctl::posix_transaction_run_evidence_store::open(
        fifo_index_path.string());
    (void)fifo_store.publish(record);
    test_support::replace_with_fifo(
        evidence_file_with_suffix(fifo_index_path, ".pci"));
    CHECK(test_support::child_reports_without_blocking([&] {
      return evidence_rejects(
          pkgctl::transaction_run_evidence_error_code::store_corrupt,
          [&] {
            (void)fifo_store.load_construction(
                record.journal(), record.dispatch(), record.attempt_session());
          });
    }));
  }

  {
    const auto fifo_object_path = temporary.path() / "evidence-fifo-object";
    std::filesystem::create_directory(fifo_object_path);
    auto fifo_store = pkgctl::posix_transaction_run_evidence_store::open(
        fifo_object_path.string());
    (void)fifo_store.publish(record);
    test_support::replace_with_fifo(
        evidence_file_with_suffix(fifo_object_path, ".pce"));
    CHECK(test_support::child_reports_without_blocking([&] {
      return evidence_rejects(
          pkgctl::transaction_run_evidence_error_code::store_corrupt,
          [&] {
            (void)fifo_store.load_construction(
                record.journal(), record.dispatch(), record.attempt_session());
          });
    }));
  }

  {
    const auto fifo_lock_path = temporary.path() / "evidence-fifo-lock";
    std::filesystem::create_directory(fifo_lock_path);
    auto fifo_store = pkgctl::posix_transaction_run_evidence_store::open(
        fifo_lock_path.string());
    CHECK(::mkfifo(
              (fifo_lock_path / ".pkgctl-run-evidence.lock").c_str(), 0444) ==
          0);
    CHECK(test_support::child_reports_without_blocking([&] {
      return evidence_rejects(
          pkgctl::transaction_run_evidence_error_code::store_contract_violation,
          [&] {
            (void)fifo_store.load_construction(
                record.journal(), record.dispatch(), record.attempt_session());
          });
    }));
  }

  flip_first_byte(evidence_file_with_suffix(selected_path, ".pce"));
  bool corrupt_object = false;
  try
  {
    (void)store.load_construction(
        record.journal(), record.dispatch(), record.attempt_session());
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    corrupt_object = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::corrupt_encoding;
  }
  CHECK(corrupt_object);
}

void check_durable_restart_reconciliation()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction,
      std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::construction)
      throw std::runtime_error(
          "fixture did not reserve a construction dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  const auto make_fixture = [](const std::filesystem::path& root) {
    test_support::initialize_state(root / "state");
    pkgstate::posix::canonical_generation_store store(
        root / "state", test_support::binding());
    const std::string payload = "restart construction payload\n";
    auto source = tool_source(sha256_text(payload), "1.0", false);
    auto transaction = transaction_session(
        source, dependency_source(), store.read(), root / "state");
    auto session = construction_session_without_inputs(
        transaction, root / "construction");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    return std::make_pair(std::move(transaction), std::move(session));
  };

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 51U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);

    const auto released = pkgctl::reconcile_reserved_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            reservation.run.progress(), reserved),
        *reservation.dispatch, run_store);
    CHECK(trace == std::vector<std::string>({"run-1"}));
    CHECK(released.record.sequence() == reserved.sequence() + 1U);
    CHECK(released.run.records().size() == 1U);
    if (released.run.records().size() == 1U)
      CHECK(released.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::released_unstarted);

    const auto repeated = pkgctl::reconcile_reserved_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            reservation.run.progress(), reserved),
        *reservation.dispatch, run_store);
    CHECK(repeated.record.identity() == released.record.identity());
    CHECK(run_store.latest().identity() == released.record.identity());
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 52U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto result = pkgctl::execute_construction(fixture.second, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    const auto completed = pkgctl::reconcile_construction_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, result, run_store);

    CHECK(trace == std::vector<std::string>({"run-1"}));
    CHECK(completed.result.identity() == result.identity());
    CHECK(completed.record.sequence() == started.sequence() + 1U);
    CHECK(completed.run.records().size() == 1U);
    if (completed.run.records().size() == 1U)
    {
      CHECK(completed.run.records().front().state() ==
            pkgctl::transaction_dispatch_state::completed);
      CHECK(completed.run.records().front().terminal_evidence() ==
            std::optional<pkgctl::session_identity>(result.identity()));
    }
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 53U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto result = pkgctl::execute_construction(fixture.second, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace, 1U);
    bool failed = false;
    try
    {
      (void)pkgctl::reconcile_construction_dispatch_durable(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, result, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(run_store.latest().identity() == started.identity());

    const auto completed = pkgctl::reconcile_construction_dispatch_durable(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, result, run_store);
    CHECK(completed.record.sequence() == started.sequence() + 1U);
  }

  {
    test_support::temporary_directory temporary;
    auto fixture = make_fixture(temporary.path());
    auto [reservation, reserved] =
        reserve_construction(fixture.first, 54U);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, fixture.second);
    auto started = reserved.successor(started_run);

    const std::string payload = "restart construction payload\n";
    pkgstate::posix::canonical_generation_store state_store(
        temporary.path() / "state", test_support::binding());
    auto foreign_transaction = transaction_session(
        tool_source(sha256_text(payload), "2.0", false),
        dependency_source(), state_store.read(), temporary.path() / "state");
    auto foreign_session = construction_session_without_inputs(
        foreign_transaction, temporary.path() / "foreign-construction");
    test_support::write(
        foreign_session.paths().local_source_root / "payload", payload);
    fixture_backend backend(backend_mode::succeed);
    pkgctl::native_construction_driver driver(backend);
    auto foreign_result = pkgctl::execute_construction(
        foreign_session, driver);

    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    bool refused = false;
    try
    {
      (void)pkgctl::reconcile_construction_dispatch_durable(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, foreign_result, run_store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(trace.empty());
  }
}


void check_run_authority_rehydration()
{
  const auto reserve_construction = [](
      const pkgctl::transaction_session& transaction,
      std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto admitted = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(marker));
    if (!reservation.dispatch || reservation.dispatch->unit().kind() !=
        pkgctl::transaction_unit_kind::construction)
      throw std::runtime_error(
          "fixture did not reserve a construction dispatch");
    auto reserved = admitted.successor(reservation.run);
    return std::make_pair(std::move(reservation), std::move(reserved));
  };

  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "authority construction payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), store.read(), temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(session.paths().local_source_root / "payload", payload);

  auto [reservation, reserved] = reserve_construction(transaction, 61U);
  fixed_progress_source progress_source(reservation.run.progress());
  auto restart = pkgctl::rehydrate_transaction_run(reserved, progress_source);
  CHECK(progress_source.calls() == 1U);
  CHECK(progress_source.requested_record() ==
        std::optional<pkgctl::session_identity>(reserved.identity()));
  CHECK(restart.record().identity() == reserved.identity());
  CHECK(restart.run().identity() == reservation.run.identity());

  construction_execution_authority_source execution_source(session);
  const auto execution =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch, execution_source);
  CHECK(execution_source.calls() == 1U);
  CHECK(execution_source.record() ==
        std::optional<pkgctl::session_identity>(reserved.identity()));
  CHECK(execution_source.run() ==
        std::optional<pkgctl::session_identity>(reservation.run.identity()));
  CHECK(execution_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(execution.kind() == pkgctl::transaction_unit_kind::construction);
  CHECK(execution.construction() != nullptr);
  CHECK(execution.check() == nullptr);
  CHECK(execution.operation() == nullptr);
  if (execution.construction())
    CHECK(execution.construction()->identity() == session.identity());

  construction_execution_authority_source repeated_source(session);
  const auto repeated =
      pkgctl::acquire_transaction_dispatch_execution_authority(
          reserved, reservation.run, *reservation.dispatch, repeated_source);
  CHECK(repeated.identity() == execution.identity());

  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  construction_execution_authority_source active_execution(session);
  bool refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        started, started_run, *reservation.dispatch, active_execution);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
  CHECK(active_execution.calls() == 0U);

  construction_execution_authority_source mismatched_execution(session);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        reserved, started_run, *reservation.dispatch, mismatched_execution);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
  CHECK(mismatched_execution.calls() == 0U);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);

  fixed_progress_source started_progress_source(started_run.progress());
  auto started_restart = pkgctl::rehydrate_transaction_run(
      started, started_progress_source);
  construction_recovery_authority_source recovery_source(result);
  const auto recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          started_restart, *reservation.dispatch, recovery_source);
  CHECK(recovery_source.calls() == 1U);
  CHECK(recovery_source.record() ==
        std::optional<pkgctl::session_identity>(started.identity()));
  CHECK(recovery_source.assessment() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery_source.dispatch() ==
        std::optional<pkgctl::session_identity>(
            reservation.dispatch->identity()));
  CHECK(recovery.disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::
            recover_construction);
  CHECK(!recovery.releases_reserved());
  CHECK(recovery.construction() != nullptr);
  CHECK(recovery.check() == nullptr);
  CHECK(recovery.operation() == nullptr);
  if (recovery.construction())
    CHECK(recovery.construction()->identity() == result.identity());

  construction_recovery_authority_source repeated_recovery_source(result);
  const auto repeated_recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              started_run.progress(), started),
          *reservation.dispatch, repeated_recovery_source);
  CHECK(repeated_recovery.identity() == recovery.identity());

  unreachable_recovery_authority_source no_evidence;
  const auto release =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          pkgctl::transaction_run_restart_checkpoint::make(
              reservation.run.progress(), reserved),
          *reservation.dispatch, no_evidence);
  CHECK(release.releases_reserved());
  CHECK(release.disposition() ==
        pkgctl::transaction_dispatch_restart_disposition::release_reserved);

  auto foreign_transaction = transaction_session(
      tool_source(sha256_text(payload), "2.0", false),
      dependency_source(), store.read(), temporary.path() / "state");
  auto foreign_session = construction_session_without_inputs(
      foreign_transaction, temporary.path() / "foreign");
  fixed_progress_source foreign_progress_source(
      pkgctl::advance_construction(reservation.run.progress(), result));
  refused = false;
  try
  {
    (void)pkgctl::rehydrate_transaction_run(
        reserved, foreign_progress_source);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_record;
  }
  CHECK(refused);
  CHECK(foreign_progress_source.calls() == 1U);

  construction_execution_authority_source foreign_execution(foreign_session);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_execution_authority(
        reserved, reservation.run, *reservation.dispatch, foreign_execution);
  }
  catch (const pkgctl::error&)
  {
    refused = true;
  }
  CHECK(refused);

  test_support::write(
      foreign_session.paths().local_source_root / "payload", payload);
  auto foreign_result = pkgctl::execute_construction(foreign_session, driver);
  construction_recovery_authority_source foreign_recovery(foreign_result);
  refused = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, foreign_recovery);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
}



void check_stored_construction_recovery()
{
  test_support::temporary_directory temporary;
  const auto state_path = temporary.path() / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "stored construction recovery payload\n";
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), "1.0", false),
      dependency_source(), state_store.read(), state_path);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(session.paths().local_source_root / "payload", payload);

  auto run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(218U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(218U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  auto reserved = admitted.successor(reservation.run);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(backend);
  auto result = pkgctl::execute_construction(session, driver);

  const auto evidence_path = temporary.path() / "evidence";
  std::filesystem::create_directory(evidence_path);
  auto evidence = pkgctl::construction_dispatch_evidence_record::admit(
      started, *reservation.dispatch, result);
  {
    auto writer = pkgctl::posix_transaction_run_evidence_store::open(
        evidence_path.string());
    CHECK(writer.publish(evidence).identity() == evidence.identity());
  }
  auto evidence_store = pkgctl::posix_transaction_run_evidence_store::open(
      evidence_path.string());

  auto checkpoint = pkgctl::transaction_run_restart_checkpoint::make(
      started_run.progress(), started);

  const auto attempt_path = temporary.path() / "attempt-evidence";
  std::filesystem::create_directory(attempt_path);
  auto attempt_store = pkgctl::posix_transaction_run_evidence_store::open(
      attempt_path.string());
  const auto attempt = pkgctl::construction_dispatch_attempt_record::admit(
      reserved, *reservation.dispatch, session);
  const auto attempt_encoding =
      pkgctl::encode_construction_dispatch_attempt(attempt);
  const auto decoded_attempt =
      pkgctl::decode_construction_dispatch_attempt(attempt_encoding);
  CHECK(decoded_attempt.identity() == attempt.identity());
  CHECK(decoded_attempt.session_encoding() == attempt.session_encoding());
  CHECK(pkgctl::encode_construction_dispatch_attempt(decoded_attempt) ==
        attempt_encoding);
  CHECK(attempt_store.publish(attempt).identity() == attempt.identity());
  pkgctl::native_transaction_dispatch_recovery_context_source attempt_context;
  pkgctl::stored_transaction_dispatch_recovery_authority_source attempt_source(
      attempt_store, attempt_context);
  auto attempt_recovery = pkgctl::acquire_transaction_dispatch_recovery_authority(
      checkpoint, *reservation.dispatch, attempt_source);
  CHECK(attempt_recovery.construction_retry() != nullptr);
  CHECK(attempt_recovery.construction() == nullptr);
  if (attempt_recovery.construction_retry())
  {
    CHECK(attempt_recovery.construction_retry()->identity() == session.identity());
    CHECK(pkgctl::encode_construction_session(
              *attempt_recovery.construction_retry()) ==
          pkgctl::encode_construction_session(session));
  }

  construction_recovery_context_source context(result);
  pkgctl::stored_transaction_dispatch_recovery_authority_source recovery_source(
      evidence_store, context);
  auto recovery = pkgctl::acquire_transaction_dispatch_recovery_authority(
      checkpoint, *reservation.dispatch, recovery_source);
  CHECK(context.calls() == 1U);
  CHECK(recovery.construction() != nullptr);
  if (recovery.construction())
  {
    CHECK(recovery.construction()->identity() == result.identity());
    CHECK(recovery.construction()->session().identity() ==
          result.session().identity());
    CHECK(recovery.construction()->materialization().identity() ==
          result.materialization().identity());
    CHECK(pkgbuild_exec::encode_build_execution_result(
              recovery.construction()->build()) ==
          pkgbuild_exec::encode_build_execution_result(result.build()));
  }

  const auto empty_path = temporary.path() / "empty-evidence";
  std::filesystem::create_directory(empty_path);
  auto empty_store = pkgctl::posix_transaction_run_evidence_store::open(
      empty_path.string());
  construction_recovery_context_source unused_context(result);
  pkgctl::stored_transaction_dispatch_recovery_authority_source missing_source(
      empty_store, unused_context);
  bool missing = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, missing_source);
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    missing = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::evidence_missing;
  }
  CHECK(missing);
  CHECK(unused_context.calls() == 0U);

  auto foreign_transaction = transaction_session(
      tool_source(sha256_text(payload), "2.0", false),
      dependency_source(), state_store.read(), state_path);
  auto foreign_session = construction_session_without_inputs(
      foreign_transaction, temporary.path() / "foreign");
  test_support::write(
      foreign_session.paths().local_source_root / "payload", payload);
  auto foreign_result = pkgctl::execute_construction(foreign_session, driver);
  construction_recovery_context_source foreign_context(foreign_result);
  pkgctl::stored_transaction_dispatch_recovery_authority_source foreign_source(
      evidence_store, foreign_context);
  bool mismatched = false;
  try
  {
    (void)pkgctl::acquire_transaction_dispatch_recovery_authority(
        pkgctl::transaction_run_restart_checkpoint::make(
            started_run.progress(), started),
        *reservation.dispatch, foreign_source);
  }
  catch (const pkgctl::transaction_run_evidence_error& problem)
  {
    mismatched = problem.code() ==
        pkgctl::transaction_run_evidence_error_code::
            recovery_context_mismatch;
  }
  CHECK(mismatched);
  CHECK(foreign_context.calls() == 1U);

  const auto retained_materialization_encoding =
      evidence.materialization_encoding();
  std::filesystem::remove_all(session.paths().local_source_root);
  std::filesystem::remove_all(session.paths().content_store_root);
  CHECK(!std::filesystem::exists(session.paths().local_source_root));
  CHECK(!std::filesystem::exists(session.paths().content_store_root));
  CHECK(!std::filesystem::exists(
      result.materialization().objects().front().object_path()));

  pkgctl::native_transaction_dispatch_recovery_context_source native_context;
  pkgctl::stored_transaction_dispatch_recovery_authority_source native_source(
      evidence_store, native_context);
  auto native_recovery =
      pkgctl::acquire_transaction_dispatch_recovery_authority(
          checkpoint, *reservation.dispatch, native_source);
  CHECK(native_recovery.construction() != nullptr);
  if (native_recovery.construction())
  {
    CHECK(native_recovery.construction()->identity() == result.identity());
    CHECK(native_recovery.construction()->session().identity() ==
          session.identity());
    CHECK(native_recovery.construction()->materialization().identity() ==
          result.materialization().identity());
    CHECK(pkgfetch::encode_source_materialization(
              native_recovery.construction()->materialization()) ==
          retained_materialization_encoding);
  }
}

void check_single_step_transaction_advancement()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "single-step construction payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction-step");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  const auto make_admitted = [&](std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  {
    auto [run, admitted] = make_admitted(71U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto advanced = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(71U),
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(advanced.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              executed_construction);
    CHECK(advanced.durable_transition_committed());
    CHECK(!advanced.external_resolution_required());
    CHECK(advanced.dispatch().has_value());
    CHECK(advanced.construction() != nullptr);
    CHECK(advanced.check() == nullptr);
    CHECK(advanced.operation() == nullptr);
    CHECK(advanced.record().sequence() == 3U);
    CHECK(advanced.record().identity() == run_store.latest().identity());
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 1U);
    CHECK(trace == std::vector<std::string>({
        "run-1", "attempt-construction", "run-2", "materialize", "build",
        "evidence-construction", "publish", "run-3"}));
  }

  {
    auto [run, admitted] = make_admitted(72U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(72U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("construction step fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    const auto released = pkgctl::advance_transaction_run_once(
        reserved.journal(), dispatch_nonce(73U),
        {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(released.disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(released.record().sequence() == 2U);
    CHECK(released.dispatch().has_value());
    CHECK(released.construction() == nullptr);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(74U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(74U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("construction recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);
    fs::remove(session.paths().build.artifact_path);
    CHECK(!fs::exists(session.paths().build.artifact_path));
    auto recovered_result =
        pkgctl::execute_construction_unpublished(session, native_driver);
    CHECK(!fs::exists(session.paths().build.artifact_path));

    fixed_progress_source progress_source(started_run.progress());
    construction_execution_authority_source execution_source(session);
    construction_recovery_authority_source recovery_source(recovered_result);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto reconciled = pkgctl::advance_transaction_run_once(
        started.journal(), dispatch_nonce(75U),
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(reconciled.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              reconciled_construction);
    CHECK(reconciled.construction() != nullptr);
    CHECK(reconciled.construction() &&
          reconciled.construction()->identity() == recovered_result.identity());
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace == std::vector<std::string>({"publish", "run-1"}));
    CHECK(fs::is_regular_file(session.paths().build.artifact_path));
  }

  {
    auto [run, admitted] = make_admitted(76U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(76U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("construction replay fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto started_run = pkgctl::start_construction_dispatch(
        reservation.run, *reservation.dispatch, session);
    auto started = reserved.successor(started_run);
    fs::remove(session.paths().build.artifact_path);
    CHECK(!fs::exists(session.paths().build.artifact_path));

    fixed_progress_source progress_source(started_run.progress());
    construction_execution_authority_source execution_source(session);
    construction_recovery_authority_source recovery_source(session);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(started, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto replayed = pkgctl::advance_transaction_run_once(
        started.journal(), dispatch_nonce(77U),
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(replayed.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              reconciled_construction);
    CHECK(replayed.construction() != nullptr);
    CHECK(recovery_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(replayed.record().sequence() == started.sequence() + 1U);
    CHECK(trace == std::vector<std::string>({
        "materialize", "build", "evidence-construction", "publish",
        "run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(78U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(78U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("stale-head fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    auto [foreign_run, foreign_admitted] = make_admitted(77U);
    fixed_progress_source progress_source(foreign_run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          foreign_admitted.journal(), dispatch_nonce(77U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_conflict;
    }
    CHECK(refused);
    CHECK(progress_source.calls() == 0U);
    CHECK(execution_source.calls() == 0U);
    CHECK(trace.empty());
  }


  {
    auto [run, admitted] = make_admitted(78U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(78U),
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(progress_source.calls() == 1U);
    CHECK(execution_source.calls() == 0U);
    CHECK(run_store.latest().identity() == admitted.identity());
    CHECK(trace.empty());
  }

  {
    auto [run, admitted] = make_admitted(79U);
    fixed_progress_source progress_source(run.progress());
    throwing_construction_execution_authority_source execution_source;
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    bool failed = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(79U),
          {progress_source, execution_source, recovery_source},
          {&native_driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      failed = std::string(problem.what()) ==
          "injected execution-authority failure";
    }
    CHECK(failed);
    CHECK(execution_source.calls() == 1U);
    CHECK(run_store.latest().sequence() == 1U);
    CHECK(run_store.latest().dispatches().size() == 1U);
    CHECK(run_store.latest().dispatches().front().state() ==
          pkgctl::transaction_dispatch_state::reserved);
    CHECK(trace == std::vector<std::string>({"run-1"}));
  }

  {
    auto [run, admitted] = make_admitted(80U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    fixture_backend failing_backend(backend_mode::fail);
    pkgctl::native_construction_driver failing_driver(failing_backend);

    const auto failed = pkgctl::advance_transaction_run_once(
        admitted.journal(), dispatch_nonce(80U),
        {progress_source, execution_source, recovery_source},
        {&failing_driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    CHECK(failed.disposition() ==
          pkgctl::transaction_run_advance_disposition::
              executed_construction);
    CHECK(failed.construction() != nullptr);
    CHECK(failed.construction() && !failed.construction()->succeeded());
    CHECK(failed.run().stopped());

    fixed_progress_source stopped_progress(failed.run().progress());
    construction_execution_authority_source unused_execution(session);
    const auto trace_before = trace;
    const auto quiescent = pkgctl::advance_transaction_run_once(
        failed.record().journal(), dispatch_nonce(81U),
        {stopped_progress, unused_execution, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    CHECK(quiescent.disposition() ==
          pkgctl::transaction_run_advance_disposition::quiescent);
    CHECK(!quiescent.durable_transition_committed());
    CHECK(!quiescent.dispatch().has_value());
    CHECK(quiescent.record().identity() == failed.record().identity());
    CHECK(unused_execution.calls() == 0U);
    CHECK(trace == trace_before);
  }

  {
    auto [run, admitted] = make_admitted(82U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    throwing_construction_driver driver;

    bool escaped = false;
    try
    {
      (void)pkgctl::advance_transaction_run_once(
          admitted.journal(), dispatch_nonce(82U),
          {progress_source, execution_source, recovery_source},
          {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      escaped = std::string(problem.what()) ==
          "driver escaped without materialization evidence";
    }
    CHECK(escaped);
    CHECK(run_store.latest().sequence() == 2U);
    CHECK(run_store.latest().dispatches().size() == 1U);
    CHECK(run_store.latest().dispatches().front().state() ==
          pkgctl::transaction_dispatch_state::started);
    CHECK(trace == std::vector<std::string>({
        "run-1", "attempt-construction", "run-2"}));
  }
}




void check_canonical_transaction_dispatch_nonce_authority()
{
  test_support::temporary_directory temporary;
  const auto state_path = temporary.path() / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  auto transaction = transaction_session(
      tool_source(sha256_text("canonical nonce payload\n"), "1.0", false),
      {}, state_store.read(), state_path);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  const auto policy = pkgctl::transaction_dispatch_policy::make(1U, 1U);
  auto run = pkgctl::transaction_run::begin(progress, policy);
  auto record = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(210U));

  pkgctl::canonical_transaction_dispatch_nonce_source source;
  const auto first = source.issue(record, run);
  const auto repeated = source.issue(record, run);
  CHECK(first == repeated);
  CHECK(first == pkgctl::canonical_transaction_dispatch_nonce(record, run));

  auto reserved = pkgctl::reserve_next(run, first);
  CHECK(reserved.dispatch.has_value());
  auto successor = record.successor(reserved.run);
  const auto next = source.issue(successor, reserved.run);
  CHECK(next != first);

  auto foreign = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(2U, 1U));
  bool rejected = false;
  try
  {
    (void)source.issue(record, foreign);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    rejected = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(rejected);
}


void check_posix_transaction_run_runtime_recovery()
{
  test_support::temporary_directory temporary;
  const auto state_path = temporary.path() / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "runtime recovered construction payload\n";
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), "1.0", false), {},
      state_store.read(), state_path);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(session.paths().local_source_root / "payload", payload);

  auto run = pkgctl::transaction_run::begin(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U));
  auto admitted = pkgctl::transaction_run_journal_record::admit(
      run, journal_nonce(219U));
  auto reservation = pkgctl::reserve_next(run, dispatch_nonce(219U));
  CHECK(reservation.dispatch.has_value());
  if (!reservation.dispatch)
    return;
  auto reserved = admitted.successor(reservation.run);
  auto started_run = pkgctl::start_construction_dispatch(
      reservation.run, *reservation.dispatch, session);
  auto started = reserved.successor(started_run);

  fixture_backend execution_backend(backend_mode::succeed);
  pkgctl::native_construction_driver driver(execution_backend);
  auto result = pkgctl::execute_construction(session, driver);
  auto evidence = pkgctl::construction_dispatch_evidence_record::admit(
      started, *reservation.dispatch, result);

  const auto run_path = temporary.path() / "run-store";
  const auto evidence_path = temporary.path() / "evidence-store";
  const auto effect_path = temporary.path() / "effect-store";
  const auto lock_path = temporary.path() / "target-locks";
  std::filesystem::create_directory(run_path);
  std::filesystem::create_directory(evidence_path);
  std::filesystem::create_directory(effect_path);
  std::filesystem::create_directory(lock_path);

  {
    auto run_store = pkgctl::posix_transaction_run_journal_store::open(
        run_path.string());
    CHECK(run_store.append(admitted).identity() == admitted.identity());
    CHECK(run_store.append(reserved).identity() == reserved.identity());
    CHECK(run_store.append(started).identity() == started.identity());
  }
  {
    auto evidence_store = pkgctl::posix_transaction_run_evidence_store::open(
        evidence_path.string());
    CHECK(evidence_store.publish(evidence).identity() == evidence.identity());
  }

  const auto materialized_object =
      result.materialization().objects().front().object_path();
  std::filesystem::remove_all(session.paths().local_source_root);
  std::filesystem::remove_all(session.paths().content_store_root);
  CHECK(!std::filesystem::exists(session.paths().local_source_root));
  CHECK(!std::filesystem::exists(session.paths().content_store_root));
  CHECK(!std::filesystem::exists(materialized_object));

  fixed_progress_source progress_source(started_run.progress());
  construction_execution_authority_source execution(session);
  unreachable_operation_execution_authority_source operation_execution;
  unreachable_operation_recovery_context_source operation_recovery;
  forbidden_runtime_archive_source archives;
  unreachable_runtime_application_backend application_backend;
  unreachable_runtime_application_journal_store application_journals;
  forbidden_recovery_execution_backend recovery_backend;

  const int run_fd = open_runtime_directory(run_path);
  const int evidence_fd = open_runtime_directory(evidence_path);
  const int effect_fd = open_runtime_directory(effect_path);
  const int lock_fd = open_runtime_directory(lock_path);
  auto runtime = pkgctl::posix_transaction_run_runtime::from_directory_fds(
      run_fd, evidence_fd, effect_fd, lock_fd,
      {progress_source, execution, operation_execution, operation_recovery,
       archives},
      {recovery_backend, recovery_backend, application_backend,
       application_journals, recovery_backend, state_store});
  CHECK(::close(run_fd) == 0);
  CHECK(::close(evidence_fd) == 0);
  CHECK(::close(effect_fd) == 0);
  CHECK(::close(lock_fd) == 0);

  const auto recovered = runtime->drive(
      admitted.journal(), pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(recovered.disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(recovered.steps().size() == 1U);
  CHECK(recovered.steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::reconciled_construction);
  CHECK(recovered.steps().front().construction() != nullptr);
  CHECK(recovered.steps().front().construction() &&
        recovered.steps().front().construction()->identity() ==
            result.identity());
  CHECK(recovered.record().sequence() == 3U);
  CHECK(recovered.run().progress().complete());
  CHECK(progress_source.calls() == 1U);
  CHECK(execution.calls() == 0U);
  CHECK(recovery_backend.capability_calls() == 0U);
  CHECK(recovery_backend.execution_calls() == 0U);
  CHECK(archives.calls() == 0U);
  CHECK(directory_entry_count(effect_path) == 0U);
  CHECK(directory_entry_count(lock_path) == 0U);
}


void check_posix_transaction_run_runtime()
{
  test_support::temporary_directory temporary;
  const auto state_path = temporary.path() / "state";
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "runtime construction payload\n";
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), "1.0", false), {},
      state_store.read(), state_path);
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "construction");
  test_support::write(session.paths().local_source_root / "payload", payload);

  fixed_progress_source progress_source(progress);
  construction_execution_authority_source execution(std::move(session));
  unreachable_operation_execution_authority_source operation_execution;
  unreachable_operation_recovery_context_source operation_recovery;
  forbidden_runtime_archive_source archives;
  fixture_backend execution_backend(backend_mode::succeed);
  unreachable_runtime_application_backend application_backend;
  unreachable_runtime_application_journal_store application_journals;

  const auto run_path = temporary.path() / "run-store";
  const auto selected_run_path = temporary.path() / "run-store-selected";
  const auto evidence_path = temporary.path() / "evidence-store";
  const auto selected_evidence_path =
      temporary.path() / "evidence-store-selected";
  const auto effect_path = temporary.path() / "effect-store";
  const auto lock_path = temporary.path() / "target-locks";
  std::filesystem::create_directory(run_path);
  std::filesystem::create_directory(evidence_path);
  std::filesystem::create_directory(effect_path);
  std::filesystem::create_directory(lock_path);

  const int run_fd = open_runtime_directory(run_path);
  const int evidence_fd = open_runtime_directory(evidence_path);
  const int effect_fd = open_runtime_directory(effect_path);
  const int lock_fd = open_runtime_directory(lock_path);
  auto runtime = pkgctl::posix_transaction_run_runtime::from_directory_fds(
      run_fd, evidence_fd, effect_fd, lock_fd,
      {progress_source, execution, operation_execution, operation_recovery,
       archives},
      {execution_backend, execution_backend, application_backend,
       application_journals, execution_backend, state_store});
  CHECK(::close(run_fd) == 0);
  CHECK(::close(evidence_fd) == 0);
  CHECK(::close(effect_fd) == 0);
  CHECK(::close(lock_fd) == 0);

  std::filesystem::rename(run_path, selected_run_path);
  std::filesystem::create_directory(run_path);
  std::filesystem::rename(evidence_path, selected_evidence_path);
  std::filesystem::create_directory(evidence_path);

  const auto result = runtime->launch(
      progress, pkgctl::transaction_dispatch_policy::make(1U, 1U),
      journal_nonce(211U),
      pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(result.origin() == pkgctl::transaction_run_launch_origin::admitted);
  CHECK(result.admission_committed());
  CHECK(result.drive().disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(result.run().progress().complete());
  CHECK(result.record().sequence() == 3U);
  CHECK(result.record().nonce() == journal_nonce(211U));
  CHECK(result.drive().steps().size() == 1U);
  CHECK(result.drive().steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::executed_construction);
  CHECK(progress_source.calls() == 1U);
  CHECK(execution.calls() == 1U);
  CHECK(archives.calls() == 0U);
  CHECK(directory_entry_count(selected_run_path) >= 4U);
  CHECK(directory_entry_count(run_path) == 0U);
  CHECK(directory_entry_count(selected_evidence_path) >= 3U);
  CHECK(directory_entry_count(evidence_path) == 0U);
  CHECK(directory_entry_count(effect_path) == 0U);
  CHECK(directory_entry_count(lock_path) == 0U);
  CHECK(std::filesystem::is_regular_file(
      temporary.path() / "construction" / "artifact" / "tool.tar"));

  progress_source.replace(result.run().progress());
  const auto completed = runtime->drive(
      result.record().journal(),
      pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(completed.disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(completed.steps().size() == 1U);
  CHECK(completed.steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::quiescent);
  CHECK(completed.record().identity() == result.record().identity());
  CHECK(progress_source.calls() == 2U);

  bool run_descriptor_refused = false;
  try
  {
    const int valid_evidence = open_runtime_directory(evidence_path);
    const int valid_effect = open_runtime_directory(effect_path);
    const int valid_lock = open_runtime_directory(lock_path);
    try
    {
      (void)pkgctl::posix_transaction_run_runtime::from_directory_fds(
          -1, valid_evidence, valid_effect, valid_lock,
          {progress_source, execution, operation_execution,
           operation_recovery, archives},
          {execution_backend, execution_backend, application_backend,
           application_journals, execution_backend, state_store});
    }
    catch (...)
    {
      (void)::close(valid_evidence);
      (void)::close(valid_effect);
      (void)::close(valid_lock);
      throw;
    }
    (void)::close(valid_evidence);
    (void)::close(valid_effect);
    (void)::close(valid_lock);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    run_descriptor_refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::store_open_failed;
  }
  CHECK(run_descriptor_refused);
}

void check_native_posix_transaction_run_runtime()
{
  test_support::temporary_directory temporary;
  const auto root = temporary.path();
  const auto state_path = root / "state";
  const auto collection = root / "collection";
  std::filesystem::create_directories(collection / "tool");
  test_support::initialize_state(state_path);
  pkgstate::posix::canonical_generation_store state_store(
      state_path, test_support::binding());

  const std::string payload = "native runtime construction payload\n";
  test_support::write(collection / "tool" / "payload", payload);
  tool_source_options source_options;
  source_options.with_build_dependency = false;
  source_options.source_document =
      (collection / "tool" / "recipe.yml").generic_string();
  auto transaction = transaction_session(
      tool_source(sha256_text(payload), std::move(source_options)), {},
      state_store.read(), state_path, false, false, false, collection);

  const auto authority_root = root / "native-authority";
  auto session_configuration =
      native_runtime_session_configuration(authority_root);

  tool_source_options target_source_options;
  target_source_options.with_build_dependency = false;
  target_source_options.source_document =
      (collection / "tool" / "recipe.yml").generic_string();
  auto target_transaction = transaction_session(
      tool_source(sha256_text(payload), std::move(target_source_options)), {},
      state_store.read(), state_path, true, false, false, collection);

  bool surplus_operation_authority_refused = false;
  try
  {
    auto surplus_operation = native_runtime_operation_configuration(
        transaction, authority_root / "surplus-operation");
    (void)pkgctl::native_transaction_run_runtime_configuration::make(
        transaction, session_configuration, std::move(surplus_operation), {});
  }
  catch (const pkgctl::native_transaction_run_runtime_error& problem)
  {
    surplus_operation_authority_refused = problem.code() ==
        pkgctl::native_transaction_run_runtime_error_code::
            invalid_configuration;
  }
  CHECK(surplus_operation_authority_refused);

  bool contradictory_root_refused = false;
  try
  {
    auto contradictory_operation =
        pkgctl::native_transaction_operation_configuration::make(
            target_transaction, package_policy(),
            {
                pkgexec::root_view_identity::from_sha256(
                    std::string(64U, '7')),
                session_configuration.roots().root_view_path,
                authority_root / "contradictory-target",
                authority_root / "contradictory-lifecycle-sessions",
                native_runtime_lifecycle_execution_identity(),
            });
    (void)pkgctl::native_transaction_run_runtime_configuration::make(
        target_transaction, session_configuration,
        std::move(contradictory_operation), {});
  }
  catch (const pkgctl::native_transaction_run_runtime_error& problem)
  {
    contradictory_root_refused = problem.code() ==
        pkgctl::native_transaction_run_runtime_error_code::
            invalid_configuration;
  }
  CHECK(contradictory_root_refused);

  const auto matrix_root = authority_root / "root-overlap-matrix";
  auto matrix_session_configuration =
      pkgctl::native_transaction_session_configuration::make(
          {
              matrix_root / "content" / "root",
              matrix_root / "construction-session" / "root",
              matrix_root / "package-output" / "root",
              matrix_root / "artifact" / "root",
              matrix_root / "installed-resource" / "root",
              matrix_root / "check-resource" / "root",
              matrix_root / "check-temporary" / "root",
              pkgexec::root_view_identity::from_sha256(
                  std::string(64U, '5')),
              matrix_root / "construction-check-root-view" / "root",
          },
          session_configuration.policy());
  const auto& session_roots = matrix_session_configuration.roots();
  const std::array<std::pair<const std::filesystem::path*, const char*>, 8>
      session_root_matrix{{
          {&session_roots.content_store_root, "content"},
          {&session_roots.construction_session_root, "construction-session"},
          {&session_roots.package_output_root, "package-output"},
          {&session_roots.artifact_root, "artifact"},
          {&session_roots.installed_resource_root, "installed-resource"},
          {&session_roots.check_resource_root, "check-resource"},
          {&session_roots.check_temporary_root, "check-temporary"},
          {&session_roots.root_view_path, "construction/check-root-view"},
      }};
  const std::array<const char*, 3> lifecycle_root_matrix{
      "lifecycle-execution", "target", "lifecycle-session"};
  const std::array<const char*, 3> overlap_relation_matrix{
      "aliases", "contains", "is-contained-by"};
  std::size_t lifecycle_overlap_cases = 0U;
  for (const auto& [session_root, session_name] : session_root_matrix)
  {
    for (std::size_t selected = 0U; selected < lifecycle_root_matrix.size();
         ++selected)
    {
      for (std::size_t relation = 0U;
           relation < overlap_relation_matrix.size(); ++relation)
      {
        auto execution = authority_root / "matrix-lifecycle-execution";
        auto target = authority_root / "matrix-target";
        auto lifecycle_session = authority_root / "matrix-lifecycle-session";
        auto operation_roots = std::array<std::filesystem::path*, 3>{
            &execution, &target, &lifecycle_session};
        switch (relation)
        {
          case 0U:
            *operation_roots[selected] = *session_root;
            break;
          case 1U:
            *operation_roots[selected] = *session_root / "nested-operation";
            break;
          case 2U:
            *operation_roots[selected] = session_root->parent_path();
            break;
          default:
            throw std::runtime_error("invalid overlap relation fixture");
        }

        bool refused = false;
        try
        {
          auto overlapping_operation =
              pkgctl::native_transaction_operation_configuration::make(
                  target_transaction, package_policy(),
                  {
                      pkgexec::root_view_identity::from_sha256(
                          std::string(64U, '8')),
                      execution,
                      target,
                      lifecycle_session,
                      native_runtime_lifecycle_execution_identity(),
                  });
          (void)pkgctl::native_transaction_run_runtime_configuration::make(
              target_transaction, matrix_session_configuration,
              std::move(overlapping_operation), {});
        }
        catch (const pkgctl::native_transaction_run_runtime_error& problem)
        {
          refused = problem.code() ==
              pkgctl::native_transaction_run_runtime_error_code::
                  invalid_configuration;
        }
        ++lifecycle_overlap_cases;
        if (!refused)
        {
          std::cerr << "native root overlap matrix accepted " << session_name
                    << ' ' << overlap_relation_matrix[relation] << ' '
                    << lifecycle_root_matrix[selected] << '\n';
          ++failures;
        }
      }
    }
  }
  CHECK(lifecycle_overlap_cases == 72U);

  auto operation_configuration = native_runtime_operation_configuration(
      target_transaction, authority_root);
  auto operation_runtime_configuration =
      pkgctl::native_transaction_run_runtime_configuration::make(
          target_transaction, session_configuration, operation_configuration,
          {});
  auto configuration =
      pkgctl::native_transaction_run_runtime_configuration::make(
          transaction, session_configuration);

  auto package_objects = pkgobject::store::open_or_create(
      root / "package-objects");
  unreachable_native_operation_specification_source operation_specifications;
  unreachable_native_effect_restart_body_source effect_restart_bodies;
  fixture_backend execution_backend(backend_mode::succeed);
  unreachable_runtime_application_backend application_backend;
  unreachable_runtime_application_journal_store application_journals;
  pkgimage::libarchive_backend archive_backend;

  const auto run_path = root / "run-store";
  const auto selected_run_path = root / "run-store-selected";
  const auto evidence_path = root / "evidence-store";
  const auto selected_evidence_path = root / "evidence-store-selected";
  const auto effect_path = root / "effect-store";
  const auto operation_lock_path = root / "operation-target-locks";
  const auto construction_lock_path = root / "construction-target-locks";
  std::filesystem::create_directory(run_path);
  std::filesystem::create_directory(evidence_path);
  std::filesystem::create_directory(effect_path);
  std::filesystem::create_directory(operation_lock_path);

  const auto missing_object_run_path = root / "missing-object-run-store";
  const auto missing_object_evidence_path = root / "missing-object-evidence-store";
  const auto missing_object_effect_path = root / "missing-object-effect-store";
  std::filesystem::create_directory(missing_object_run_path);
  std::filesystem::create_directory(missing_object_evidence_path);
  std::filesystem::create_directory(missing_object_effect_path);
  bool missing_package_object_authority_refused = false;
  try
  {
    auto missing_object_configuration =
        pkgctl::native_transaction_run_runtime_configuration::make(
            transaction, session_configuration);
    auto missing_object_runtime =
        pkgctl::native_posix_transaction_run_runtime::open(
            {missing_object_run_path, missing_object_evidence_path,
             missing_object_effect_path},
            std::move(missing_object_configuration), {},
            {&execution_backend, &execution_backend, nullptr, archive_backend});
    (void)missing_object_runtime->launch(
        pkgctl::transaction_dispatch_policy::make(1U, 1U),
        journal_nonce(210U), pkgctl::transaction_run_drive_policy::make(1U));
  }
  catch (const std::runtime_error& problem)
  {
    missing_package_object_authority_refused =
        std::string(problem.what()) ==
        "native package-object authority is unavailable for fresh construction";
  }
  CHECK(missing_package_object_authority_refused);

  bool package_object_session_overlap_refused = false;
  {
    auto overlapping_sessions = native_runtime_session_configuration(
        authority_root / "package-object-session-overlap");
    auto overlapping_configuration =
        pkgctl::native_transaction_run_runtime_configuration::make(
            transaction, overlapping_sessions);
    auto overlapping_objects = pkgobject::store::open_or_create(
        overlapping_sessions.roots().installed_resource_root);
    try
    {
      (void)pkgctl::native_posix_transaction_run_runtime::open(
          {run_path, evidence_path, effect_path},
          std::move(overlapping_configuration), {},
          {&execution_backend, &execution_backend, &overlapping_objects,
           archive_backend});
    }
    catch (const pkgctl::native_transaction_run_runtime_error& problem)
    {
      package_object_session_overlap_refused = problem.code() ==
          pkgctl::native_transaction_run_runtime_error_code::
              invalid_configuration;
    }
  }
  CHECK(package_object_session_overlap_refused);

  bool package_object_runtime_overlap_refused = false;
  {
    const auto overlap_path = root / "package-object-run-overlap";
    std::filesystem::create_directory(overlap_path);
    auto overlapping_objects = pkgobject::store::open_or_create(overlap_path);
    auto overlapping_configuration =
        pkgctl::native_transaction_run_runtime_configuration::make(
            transaction, session_configuration);
    try
    {
      (void)pkgctl::native_posix_transaction_run_runtime::open(
          {overlap_path, evidence_path, effect_path},
          std::move(overlapping_configuration), {},
          {&execution_backend, &execution_backend, &overlapping_objects,
           archive_backend});
    }
    catch (const pkgctl::native_transaction_run_runtime_error& problem)
    {
      package_object_runtime_overlap_refused = problem.code() ==
          pkgctl::native_transaction_run_runtime_error_code::directory_overlap;
    }
  }
  CHECK(package_object_runtime_overlap_refused);

  bool overlap_refused = false;
  try
  {
    (void)pkgctl::native_posix_transaction_run_runtime::open(
        {run_path, run_path, effect_path, operation_lock_path},
        operation_runtime_configuration,
        {operation_specifications, effect_restart_bodies},
        {&execution_backend, &execution_backend, application_backend,
         application_journals, &execution_backend, state_store, &package_objects,
         archive_backend});
  }
  catch (const pkgctl::native_transaction_run_runtime_error& problem)
  {
    overlap_refused = problem.code() ==
        pkgctl::native_transaction_run_runtime_error_code::directory_overlap;
  }
  CHECK(overlap_refused);

  bool descriptor_alias_refused = false;
  const int run_fd = open_runtime_directory(run_path);
  const int effect_fd = open_runtime_directory(effect_path);
  try
  {
    (void)pkgctl::native_posix_transaction_run_runtime::from_directory_fds(
        run_fd, run_fd, effect_fd, configuration, {},
        {&execution_backend, &execution_backend, &package_objects, archive_backend});
  }
  catch (const pkgctl::native_transaction_run_runtime_error& problem)
  {
    descriptor_alias_refused = problem.code() ==
        pkgctl::native_transaction_run_runtime_error_code::directory_overlap;
  }
  CHECK(::close(run_fd) == 0);
  CHECK(::close(effect_fd) == 0);
  CHECK(descriptor_alias_refused);

  bool surplus_target_lock_refused = false;
  try
  {
    (void)pkgctl::native_posix_transaction_run_runtime::open(
        {run_path, evidence_path, effect_path, operation_lock_path},
        configuration, {},
        {&execution_backend, &execution_backend, &package_objects, archive_backend});
  }
  catch (const pkgctl::native_transaction_run_runtime_error& problem)
  {
    surplus_target_lock_refused = problem.code() ==
        pkgctl::native_transaction_run_runtime_error_code::
            invalid_configuration;
  }
  CHECK(surplus_target_lock_refused);

  auto runtime = pkgctl::native_posix_transaction_run_runtime::open(
      {run_path, evidence_path, effect_path}, std::move(configuration),
      {},
      {&execution_backend, &execution_backend, &package_objects, archive_backend});

  std::filesystem::rename(run_path, selected_run_path);
  std::filesystem::create_directory(run_path);
  std::filesystem::rename(evidence_path, selected_evidence_path);
  std::filesystem::create_directory(evidence_path);

  const auto result = runtime->launch(
      pkgctl::transaction_dispatch_policy::make(1U, 1U),
      journal_nonce(212U), pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(result.origin() == pkgctl::transaction_run_launch_origin::admitted);
  CHECK(result.admission_committed());
  CHECK(result.drive().disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(result.run().progress().complete());
  CHECK(result.record().sequence() == 3U);
  CHECK(result.record().nonce() == journal_nonce(212U));
  CHECK(result.drive().steps().size() == 1U);
  CHECK(result.drive().steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::executed_construction);
  CHECK(operation_specifications.calls() == 0U);
  CHECK(effect_restart_bodies.calls() == 0U);
  CHECK(directory_entry_count(selected_run_path) >= 4U);
  CHECK(directory_entry_count(run_path) == 0U);
  CHECK(directory_entry_count(selected_evidence_path) >= 3U);
  CHECK(directory_entry_count(evidence_path) == 0U);
  CHECK(directory_entry_count(effect_path) == 0U);
  CHECK(directory_entry_count(operation_lock_path) == 0U);
  CHECK(!std::filesystem::exists(construction_lock_path));

  std::size_t artifacts = 0U;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           authority_root / "artifacts"))
    if (entry.is_regular_file() && entry.path().extension() == ".tar")
      ++artifacts;
  CHECK(artifacts == 1U);

  // Successful native construction is not complete until the exact sealed
  // archive is durably available from the package-object provider.
  const auto* completed_construction =
      result.drive().steps().front().construction();
  CHECK(completed_construction != nullptr);
  if (completed_construction != nullptr)
  {
    const auto& artifact =
        *completed_construction->build().build().artifact();
    const auto content = pkgimage::complete_archive_digest::parse(
        "v1:sha256:" + artifact.complete_digest().hex());
    const auto retained = package_objects.require(content);
    CHECK(retained.content() == content);
    CHECK(retained.byte_count() == artifact.byte_count());
    CHECK(retained.path() !=
          completed_construction->session().paths().build.artifact_path);
  }

  const auto completed = runtime->drive(
      result.record().journal(),
      pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(completed.disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(completed.steps().size() == 1U);
  CHECK(completed.steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::quiescent);
  CHECK(completed.record().identity() == result.record().identity());
  CHECK(completed.run().progress().identity() ==
        result.run().progress().identity());
  CHECK(operation_specifications.calls() == 0U);
  CHECK(effect_restart_bodies.calls() == 0U);

  runtime.reset();
  auto terminal_configuration =
      pkgctl::native_transaction_run_runtime_configuration::make(
          transaction, session_configuration);
  auto terminal_without_package_objects =
      pkgctl::native_posix_transaction_run_runtime::open(
          {selected_run_path, selected_evidence_path, effect_path},
          std::move(terminal_configuration), {},
          {nullptr, nullptr, nullptr, archive_backend});
  const auto terminal_without_resource_authority =
      terminal_without_package_objects->drive(
          result.record().journal(),
          pkgctl::transaction_run_drive_policy::make(1U));
  CHECK(terminal_without_resource_authority.disposition() ==
        pkgctl::transaction_run_drive_disposition::completed);
  CHECK(terminal_without_resource_authority.steps().size() == 1U);
  CHECK(terminal_without_resource_authority.steps().front().disposition() ==
        pkgctl::transaction_run_advance_disposition::quiescent);
}


void check_durable_transaction_run_admission()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  auto transaction = transaction_session(
      tool_source(sha256_text("admission payload\n"), "1.0", false),
      dependency_source(), state_store.read(), temporary.path() / "state");
  auto progress = pkgctl::transaction_progress::begin(transaction);
  auto policy = pkgctl::transaction_dispatch_policy::make(1U, 1U);
  const auto expected_run = pkgctl::transaction_run::begin(progress, policy);

  {
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(101U, trace);
    admission_run_store store(trace);

    const auto admitted = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>({expected_run.identity()}));
    CHECK(store.append_calls() == 1U);
    CHECK(store.latest().has_value());
    CHECK(admitted.record.sequence() == 0U);
    CHECK(!admitted.record.previous().has_value());
    CHECK(admitted.record.dispatches().empty());
    CHECK(admitted.record.run() == admitted.run.identity());
    CHECK(admitted.record.progress() == admitted.run.progress().identity());
    CHECK(admitted.run.identity() == expected_run.identity());
    CHECK(store.latest() &&
          admitted.record.identity() == store.latest()->identity());

    const auto repeated = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(repeated.record.identity() == admitted.record.identity());
    CHECK(repeated.run.identity() == admitted.run.identity());
    CHECK(nonces.calls() == 2U);
    CHECK(nonces.runs() == std::vector<pkgctl::session_identity>(
          2U, expected_run.identity()));
    CHECK(store.append_calls() == 2U);
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source refusing(102U, trace, true);
    admission_run_store store(trace);
    bool refused = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, refusing, store);
    }
    catch (const std::runtime_error& problem)
    {
      refused = std::string(problem.what()) ==
          "injected run-nonce refusal";
    }
    CHECK(refused);
    CHECK(trace == std::vector<std::string>({"nonce"}));
    CHECK(store.append_calls() == 0U);
    CHECK(!store.latest().has_value());
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(103U, trace);
    admission_run_store store(trace, 1U);
    bool failed = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, nonces, store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
    CHECK(!store.latest().has_value());

    const auto retried = pkgctl::admit_transaction_run(
        progress, policy, nonces, store);
    CHECK(retried.record.sequence() == 0U);
    CHECK(nonces.calls() == 2U);
    CHECK(nonces.runs() == std::vector<pkgctl::session_identity>(
          2U, expected_run.identity()));
    CHECK(store.append_calls() == 2U);
  }

  {
    auto foreign_run = pkgctl::transaction_run::begin(
        progress, pkgctl::transaction_dispatch_policy::make(2U, 1U));
    auto foreign_record = pkgctl::transaction_run_journal_record::admit(
        foreign_run, journal_nonce(104U));
    std::vector<std::string> trace;
    replay_run_nonce_source nonces(105U, trace);
    admission_run_store store(trace, 0U, foreign_record);
    bool rejected = false;
    try
    {
      (void)pkgctl::admit_transaction_run(
          progress, policy, nonces, store);
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_contract_violation;
    }
    CHECK(rejected);
    CHECK(trace == std::vector<std::string>({"nonce", "append"}));
  }
}

void check_bounded_serial_transaction_drive()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "bounded drive payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  auto session = construction_session_without_inputs(
      transaction, temporary.path() / "drive-construction");
  test_support::write(
      session.paths().local_source_root / "payload", payload);

  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  const auto make_admitted = [&](std::uint8_t marker) {
    auto run = pkgctl::transaction_run::begin(
        pkgctl::transaction_progress::begin(transaction),
        pkgctl::transaction_dispatch_policy::make(1U, 1U));
    auto record = pkgctl::transaction_run_journal_record::admit(
        run, journal_nonce(marker));
    return std::make_pair(std::move(run), std::move(record));
  };

  {
    auto [run, admitted] = make_admitted(91U);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(91U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(4U),
        nonces, {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(driven.terminal());
    CHECK(!driven.external_resolution_required());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.durable_step_count() == 1U);
    CHECK(driven.last().disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_construction);
    CHECK(driven.run().progress().complete());
    CHECK(driven.record().identity() == run_store.latest().identity());
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.records() ==
          std::vector<pkgctl::session_identity>({admitted.identity()}));
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>({run.identity()}));
  }

  {
    auto [run, admitted] = make_admitted(95U);
    auto failure_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-failure");
    test_support::write(
        failure_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(failure_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(95U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    fixture_backend failing_backend(backend_mode::fail);
    pkgctl::native_construction_driver failing_driver(failing_backend);

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(3U),
        nonces, {progress_source, execution_source, recovery_source},
        {&failing_driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::stopped_after_failure);
    CHECK(driven.terminal());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.run().stopped());
    CHECK(driven.run().progress().failed());
    CHECK(nonces.calls() == 1U);
  }

  {
    auto [run, admitted] = make_admitted(92U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(92U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("drive recovery fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);

    auto recovery_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-recovery");
    test_support::write(
        recovery_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(recovery_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(93U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto driven = pkgctl::drive_transaction_run(
        reserved.journal(), pkgctl::transaction_run_drive_policy::make(3U),
        nonces, {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(driven.steps().size() == 2U);
    CHECK(driven.steps()[0].disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(driven.steps()[1].disposition() ==
          pkgctl::transaction_run_advance_disposition::executed_construction);
    CHECK(nonces.calls() == 1U);
    CHECK(nonces.records().size() == 1U);
    CHECK(nonces.records().front() == driven.steps()[0].record().identity());
  }

  {
    auto [run, admitted] = make_admitted(96U);
make_admitted(96U);
    auto retry_session = construction_session_without_inputs(
        transaction, temporary.path() / "drive-retry");
    test_support::write(
        retry_session.paths().local_source_root / "payload", payload);
    fixed_progress_source progress_source(run.progress());
    construction_execution_authority_source execution_source(retry_session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(96U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(admitted, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    for (std::size_t attempt = 0U; attempt < 2U; ++attempt)
    {
      bool refused = false;
      try
      {
        (void)pkgctl::drive_transaction_run(
            admitted.journal(),
            pkgctl::transaction_run_drive_policy::make(1U), nonces,
            {progress_source, execution_source, recovery_source},
            {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
      }
      catch (const pkgctl::transaction_run_journal_error& problem)
      {
        refused = problem.code() ==
            pkgctl::transaction_run_journal_error_code::invalid_transition;
      }
      CHECK(refused);
      CHECK(run_store.latest().identity() == admitted.identity());
    }

    CHECK(nonces.calls() == 2U);
    CHECK(nonces.records() ==
          std::vector<pkgctl::session_identity>(2U, admitted.identity()));
    CHECK(nonces.runs() ==
          std::vector<pkgctl::session_identity>(2U, run.identity()));
    CHECK(trace.empty());

    const auto driven = pkgctl::drive_transaction_run(
        admitted.journal(), pkgctl::transaction_run_drive_policy::make(1U),
        nonces, {progress_source, execution_source, recovery_source},
        {&native_driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(nonces.calls() == 3U);
    CHECK(nonces.records().back() == admitted.identity());
    CHECK(nonces.runs().back() == run.identity());
  }

  {
    auto [run, admitted] = make_admitted(94U);
    auto reservation = pkgctl::reserve_next(run, dispatch_nonce(94U));
    CHECK(reservation.dispatch.has_value());
    if (!reservation.dispatch)
      throw std::runtime_error("drive budget fixture did not reserve");
    auto reserved = admitted.successor(reservation.run);
    fixed_progress_source progress_source(reservation.run.progress());
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    head_derived_nonce_source nonces(94U);
    std::vector<std::string> trace;
    run_execute_support::sequenced_run_store run_store(reserved, trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    const auto driven = pkgctl::drive_transaction_run(
        reserved.journal(), pkgctl::transaction_run_drive_policy::make(1U),
        nonces, {progress_source, execution_source, recovery_source},
        {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(driven.disposition() ==
          pkgctl::transaction_run_drive_disposition::step_limit_reached);
    CHECK(!driven.terminal());
    CHECK(driven.steps().size() == 1U);
    CHECK(driven.last().disposition() ==
          pkgctl::transaction_run_advance_disposition::released_reserved);
    CHECK(driven.durable_step_count() == 1U);
    CHECK(nonces.calls() == 0U);
  }

  bool refused = false;
  try
  {
    (void)pkgctl::transaction_run_drive_policy::make(0U);
  }
  catch (const pkgctl::transaction_run_journal_error& problem)
  {
    refused = problem.code() ==
        pkgctl::transaction_run_journal_error_code::invalid_transition;
  }
  CHECK(refused);
}


void check_restart_safe_transaction_launch()
{
  test_support::temporary_directory temporary;
  test_support::initialize_state(temporary.path() / "state");
  pkgstate::posix::canonical_generation_store state_store(
      temporary.path() / "state", test_support::binding());
  const std::string payload = "restart-safe launch payload\n";
  auto source = tool_source(sha256_text(payload), "1.0", false);
  auto transaction = transaction_session(
      source, dependency_source(), state_store.read(),
      temporary.path() / "state");
  const auto initial = pkgctl::transaction_progress::begin(transaction);
  const auto policy = pkgctl::transaction_dispatch_policy::make(1U, 1U);
  fixture_backend backend(backend_mode::succeed);
  pkgctl::native_construction_driver native_driver(backend);

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-construction");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(111U, trace);
    head_derived_nonce_source dispatch_nonces(111U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    const auto launched = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(4U),
        run_nonces, dispatch_nonces,
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(launched.origin() ==
          pkgctl::transaction_run_launch_origin::admitted);
    CHECK(launched.admission_committed());
    CHECK(launched.starting_record().sequence() == 0U);
    CHECK(launched.drive().disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(launched.run().progress().complete());
    CHECK(launched.record().identity() == run_store.latest().identity());
    CHECK(run_nonces.calls() == 1U);
    CHECK(dispatch_nonces.calls() == 1U);
    CHECK(run_store.append_calls() == 4U);

    fixed_progress_source resumed_progress(launched.run().progress());
    construction_execution_authority_source resumed_execution(session);
    unreachable_recovery_authority_source resumed_recovery;
    const auto append_count = run_store.append_calls();
    const auto resumed = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {resumed_progress, resumed_execution, resumed_recovery},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});

    CHECK(resumed.origin() ==
          pkgctl::transaction_run_launch_origin::resumed);
    CHECK(!resumed.admission_committed());
    CHECK(resumed.starting_record().identity() == launched.record().identity());
    CHECK(resumed.record().identity() == launched.record().identity());
    CHECK(resumed.drive().disposition() ==
          pkgctl::transaction_run_drive_disposition::completed);
    CHECK(resumed.drive().steps().size() == 1U);
    CHECK(resumed.drive().last().disposition() ==
          pkgctl::transaction_run_advance_disposition::quiescent);
    CHECK(run_store.append_calls() == append_count);
    CHECK(run_nonces.calls() == 2U);
    CHECK(dispatch_nonces.calls() == 1U);
  }

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-admission-retry");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(112U, trace);
    head_derived_nonce_source dispatch_nonces(112U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace, 1U);
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    tracing_construction_driver driver(native_driver, trace);

    bool failed = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      failed = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_write_failed;
    }
    CHECK(failed);
    CHECK(run_store.append_calls() == 1U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
    CHECK(std::find(trace.begin(), trace.end(), "materialize") == trace.end());
    CHECK(std::find(trace.begin(), trace.end(), "build") == trace.end());

    const auto retried = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {progress_source, execution_source, recovery_source},
        {&driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    CHECK(retried.admission_committed());
    CHECK(retried.run().progress().complete());
    CHECK(run_nonces.calls() == 2U);
  }

  {
    auto session = construction_session_without_inputs(
        transaction, temporary.path() / "launch-drive-retry");
    test_support::write(
        session.paths().local_source_root / "payload", payload);
    std::vector<std::string> trace;
    replay_run_nonce_source run_nonces(113U, trace);
    head_derived_nonce_source dispatch_nonces(113U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(session);
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      refused = problem.code() ==
          pkgctl::transaction_run_journal_error_code::invalid_transition;
    }
    CHECK(refused);
    CHECK(run_store.latest().sequence() == 0U);
    CHECK(run_store.append_calls() == 1U);
    CHECK(dispatch_nonces.calls() == 1U);

    fixed_progress_source retry_progress(initial);
    construction_execution_authority_source retry_execution(session);
    unreachable_recovery_authority_source retry_recovery;
    const auto retried = pkgctl::launch_transaction_run(
        initial, policy, pkgctl::transaction_run_drive_policy::make(2U),
        run_nonces, dispatch_nonces,
        {retry_progress, retry_execution, retry_recovery},
        {&native_driver, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    CHECK(retried.origin() ==
          pkgctl::transaction_run_launch_origin::resumed);
    CHECK(retried.starting_record().sequence() == 0U);
    CHECK(retried.run().progress().complete());
    CHECK(run_store.append_calls() == 4U);
  }

  {
    std::vector<std::string> trace;
    replay_run_nonce_source refusing(114U, trace, true);
    head_derived_nonce_source dispatch_nonces(114U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(
        construction_session_without_inputs(
            transaction, temporary.path() / "launch-refused"));
    unreachable_recovery_authority_source recovery_source;
    launch_run_store run_store(trace);
    run_execute_support::sequenced_evidence_store evidence_store(trace);

    bool refused = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          refusing, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const std::runtime_error& problem)
    {
      refused = std::string(problem.what()) ==
          "injected run-nonce refusal";
    }
    CHECK(refused);
    CHECK(run_store.load_calls() == 0U);
    CHECK(run_store.append_calls() == 0U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
  }

  {
    auto foreign_run = pkgctl::transaction_run::begin(
        initial, pkgctl::transaction_dispatch_policy::make(2U, 1U));
    auto foreign_record = pkgctl::transaction_run_journal_record::admit(
        foreign_run, journal_nonce(115U));
    foreign_launch_head_store run_store(std::move(foreign_record));
    std::vector<std::string> trace;
    run_execute_support::sequenced_evidence_store evidence_store(trace);
    replay_run_nonce_source run_nonces(116U, trace);
    head_derived_nonce_source dispatch_nonces(116U);
    fixed_progress_source progress_source(initial);
    construction_execution_authority_source execution_source(
        construction_session_without_inputs(
            transaction, temporary.path() / "launch-foreign"));
    unreachable_recovery_authority_source recovery_source;

    bool rejected = false;
    try
    {
      (void)pkgctl::launch_transaction_run(
          initial, policy, pkgctl::transaction_run_drive_policy::make(1U),
          run_nonces, dispatch_nonces,
          {progress_source, execution_source, recovery_source},
          {nullptr, nullptr, nullptr}, {run_store, evidence_store, nullptr});
    }
    catch (const pkgctl::transaction_run_journal_error& problem)
    {
      rejected = problem.code() ==
          pkgctl::transaction_run_journal_error_code::store_contract_violation;
    }
    CHECK(rejected);
    CHECK(run_store.load_calls() == 1U);
    CHECK(run_store.append_calls() == 0U);
    CHECK(progress_source.calls() == 0U);
    CHECK(dispatch_nonces.calls() == 0U);
  }
}


} // namespace

int main()
{
  try
  {
    check_success();
    check_install_preparation();
    check_check_progression();
    check_failed_progression();
    check_failed_build();
    check_transitive_build_only_check_authority();
    check_admission();
    check_identity_and_driver_contract();
    check_durable_dispatch_execution();
    check_transaction_run_evidence_storage();
    check_durable_restart_reconciliation();
    check_run_authority_rehydration();
    check_stored_construction_recovery();
    check_single_step_transaction_advancement();
    check_canonical_transaction_dispatch_nonce_authority();
    check_posix_transaction_run_runtime_recovery();
    check_posix_transaction_run_runtime();
    check_native_posix_transaction_run_runtime();
    check_durable_transaction_run_admission();
    check_bounded_serial_transaction_drive();
    check_restart_safe_transaction_launch();
  }
  catch (const std::exception& value)
  {
    std::cerr << "unexpected exception: " << value.what() << '\n';
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
