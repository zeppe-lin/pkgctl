// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/target_observation.h>

#include <pkgctl/identity.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pkgctl {
namespace {

[[noreturn]] void invalid_authority(const std::string& message)
{
  throw transaction_run_journal_error(
      transaction_run_journal_error_code::invalid_transition, message);
}

[[nodiscard]] pkgplan::filesystem_object_kind planner_kind(
    pkgapply::completed_object_kind kind)
{
  switch (kind)
  {
    case pkgapply::completed_object_kind::regular:
      return pkgplan::filesystem_object_kind::regular;
    case pkgapply::completed_object_kind::directory:
      return pkgplan::filesystem_object_kind::directory;
    case pkgapply::completed_object_kind::symlink:
      return pkgplan::filesystem_object_kind::symlink;
    case pkgapply::completed_object_kind::fifo:
      return pkgplan::filesystem_object_kind::fifo;
    case pkgapply::completed_object_kind::character_device:
      return pkgplan::filesystem_object_kind::character_device;
    case pkgapply::completed_object_kind::block_device:
      return pkgplan::filesystem_object_kind::block_device;
    case pkgapply::completed_object_kind::socket:
      return pkgplan::filesystem_object_kind::socket;
    case pkgapply::completed_object_kind::other:
      return pkgplan::filesystem_object_kind::other;
  }
  throw std::runtime_error("target observer returned an invalid object kind");
}

template<typename Value>
[[nodiscard]] const Value& required_fact(
    const pkgapply::qualified_fact<Value>& fact,
    std::string_view name)
{
  if (fact.state() != pkgapply::fact_state::known || !fact.value())
    throw std::runtime_error(
        "target observation lacks required " + std::string(name));
  return *fact.value();
}

[[nodiscard]] pkgplan::target_path_observation planner_observation(
    const pkgapply::application_path_observation& observation)
{
  if (observation.state() == pkgapply::fact_state::not_applicable)
    return pkgplan::target_path_observation::absent(observation.path());
  if (observation.state() != pkgapply::fact_state::known ||
      !observation.object())
    throw std::runtime_error(
        "native operation requires complete target observation");
  const auto& object = *observation.object();
  if (object.completeness() != pkgapply::object_fact_completeness::complete)
    throw std::runtime_error(
        "native operation refuses partial target object facts");

  std::optional<std::uint64_t> size;
  if (object.size().state() == pkgapply::fact_state::known)
    size = required_fact(object.size(), "object size");
  std::optional<pkgplan::object_timestamp> mtime;
  if (object.mtime().state() == pkgapply::fact_state::known)
  {
    const auto& value = required_fact(object.mtime(), "object timestamp");
    mtime.emplace(value.seconds, value.nanoseconds);
  }
  std::optional<pkgplan::filesystem_regular_content_identity> content;
  if (object.regular_content().state() == pkgapply::fact_state::known)
  {
    content = pkgplan::filesystem_regular_content_identity::parse(
        required_fact(object.regular_content(), "regular content").string());
  }
  std::optional<std::string> symlink;
  if (object.symlink_target().state() == pkgapply::fact_state::known)
    symlink = required_fact(object.symlink_target(), "symlink target");
  std::optional<pkgplan::device_number> device;
  if (object.device().state() == pkgapply::fact_state::known)
  {
    const auto& value = required_fact(object.device(), "device number");
    device.emplace(value.major, value.minor);
  }

  return pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          object.path(),
          pkgplan::filesystem_object_metadata(
              planner_kind(object.kind()),
              required_fact(object.mode(), "object mode"),
              required_fact(object.uid(), "object uid"),
              required_fact(object.gid(), "object gid"), std::move(size),
              std::move(mtime), std::move(content), std::move(symlink),
              std::move(device))));
}

void validate_authority(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch)
{
  if (record.transaction() != progress.transaction().identity() ||
      record.progress() != progress.identity() ||
      dispatch.reserved_from_progress() != progress.identity() ||
      dispatch.reserved_state() != progress.current_state().identity())
  {
    invalid_authority(
        "native target observation names inconsistent transaction authority");
  }
  if (dispatch.unit().kind() != transaction_unit_kind::operation)
    invalid_authority(
        "native target observation requires an operation dispatch");

  const auto found = std::find_if(
      record.dispatches().begin(), record.dispatches().end(),
      [&](const auto& candidate) {
        return candidate.dispatch().identity() == dispatch.identity();
      });
  if (found == record.dispatches().end() ||
      found->dispatch().identity() != dispatch.identity())
    invalid_authority(
        "native target observation lacks its durable dispatch record");
  if (found->state() != transaction_dispatch_state::reserved)
    invalid_authority(
        "native target observation requires a reserved dispatch");
}

[[nodiscard]] pkgplan::observation_set_identity observation_identity(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgplan::target_system_context_identity& target,
    const std::vector<pkgplan::target_path_observation>& observations,
    const std::vector<pkgapply::application_backend_evidence_identity>& evidence)
{
  std::vector<std::string> fields{
      record.identity().hex(), progress.identity().hex(),
      dispatch.identity().hex(), target.string()};
  for (const auto& planned : observations)
  {
    fields.push_back(planned.path().string());
    fields.push_back(planned.is_present() ? "present" : "absent");
    if (const auto* object = planned.object())
    {
      fields.push_back(
          std::to_string(static_cast<unsigned int>(object->kind())));
      fields.push_back(std::to_string(object->mode()));
      fields.push_back(std::to_string(object->uid()));
      fields.push_back(std::to_string(object->gid()));
      fields.push_back(
          object->size() ? "size:" + std::to_string(*object->size())
                         : "size:-");
      fields.push_back(
          object->mtime()
              ? "mtime:" + std::to_string(object->mtime()->seconds()) + ":" +
                    std::to_string(object->mtime()->nanoseconds())
              : "mtime:-");
      fields.push_back(
          object->regular_content()
              ? "content:" + object->regular_content()->string()
              : "content:-");
      fields.push_back(
          object->symlink_target()
              ? "symlink:" + *object->symlink_target()
              : "symlink:-");
      fields.push_back(
          object->device()
              ? "device:" + std::to_string(object->device()->major()) + ":" +
                    std::to_string(object->device()->minor())
              : "device:-");
    }
  }
  for (const auto& item : evidence)
    fields.push_back(item.string());

  return pkgplan::observation_set_identity::parse(
      "v1:sha256:" +
      make_session_identity("pkgctl/native-target-observations/1", fields).hex());
}

} // namespace

pkgplan::target_observation_set observe_native_target_paths(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgplan::target_system_context_identity& target,
    pkgapply::posix::application_target_observer& observer,
    std::vector<pkgplan::package_path> requested,
    std::vector<pkgapply::posix::target_hardlink_expectation> hardlinks)
{
  validate_authority(record, progress, dispatch);
  auto batch = observer.observe(std::move(requested), std::move(hardlinks));

  std::vector<pkgplan::target_path_observation> observations;
  observations.reserve(batch.observations().size());
  for (const auto& value : batch.observations())
    observations.push_back(planner_observation(value));

  auto identity = observation_identity(
      record, progress, dispatch, target, observations, batch.evidence());
  return pkgplan::target_observation_set(
      std::move(identity), target, pkgplan::fact_set_completeness::complete,
      std::move(observations));
}

} // namespace pkgctl
