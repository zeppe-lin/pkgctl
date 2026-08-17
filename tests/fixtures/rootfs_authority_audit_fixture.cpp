// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <libpkgaudit/audit.h>
#include <libpkgaudit/inventory.h>
#include <libpkgimage/entry_selection.h>
#include <libpkgimage/libarchive_backend.h>
#include <libpkgimage/package_path.h>
#include <libpkgimage/payload_sink.h>
#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgstate/installed_package.h>
#include <libpkgstate/owned_entry.h>
#include <libpkgstate/package_path.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct package_authority final {
  std::string name;
  bool installed = false;
  pkgimage::inspected_package_image inspection;
};

class collecting_sink final : public pkgimage::payload_sink {
public:
  explicit collecting_sink(pkgimage::entry_id expected) : expected_(expected)
  {
  }

  void begin(const pkgimage::package_entry& entry) override
  {
    if (began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected rootfs marker replay begin");
    began_ = true;
  }

  void write(const pkgimage::package_entry& entry,
             const std::byte* data,
             std::size_t size) override
  {
    if (!began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected rootfs marker replay write");
    payload_.append(reinterpret_cast<const char*>(data), size);
  }

  void end(const pkgimage::package_entry& entry) override
  {
    if (!began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected rootfs marker replay end");
    ended_ = true;
  }

  [[nodiscard]] bool complete() const noexcept
  {
    return began_ && ended_;
  }

  [[nodiscard]] const std::string& payload() const noexcept
  {
    return payload_;
  }

private:
  pkgimage::entry_id expected_;
  bool began_ = false;
  bool ended_ = false;
  std::string payload_;
};

[[nodiscard]] std::string archive_digest(std::string_view raw)
{
  if (raw.size() != 64U)
    throw std::invalid_argument("archive SHA-256 must contain 64 hex digits");
  return "v1:sha256:" + std::string(raw);
}

[[nodiscard]] pkgstate::owned_object_kind
state_kind(pkgimage::entry_type type)
{
  switch (type)
  {
  case pkgimage::entry_type::regular:
  case pkgimage::entry_type::hardlink:
    return pkgstate::owned_object_kind::regular;
  case pkgimage::entry_type::directory:
    return pkgstate::owned_object_kind::directory;
  case pkgimage::entry_type::symlink:
    return pkgstate::owned_object_kind::symlink;
  case pkgimage::entry_type::fifo:
    return pkgstate::owned_object_kind::fifo;
  case pkgimage::entry_type::character_device:
    return pkgstate::owned_object_kind::character_device;
  case pkgimage::entry_type::block_device:
    return pkgstate::owned_object_kind::block_device;
  }
  throw std::runtime_error("unsupported package-image object kind");
}

[[nodiscard]] pkgaudit::expected_object_type
audit_kind(pkgimage::entry_type type) noexcept
{
  return type == pkgimage::entry_type::directory
      ? pkgaudit::expected_object_type::directory
      : pkgaudit::expected_object_type::non_directory;
}

[[noreturn]] void state_mismatch(std::string_view package,
                                 std::string_view path,
                                 std::string_view field)
{
  throw std::runtime_error(
      std::string(package) + ": state/image mismatch at " +
      std::string(path) + ": " + std::string(field));
}

void require_state_entry(std::string_view package,
                         const pkgimage::package_image& image,
                         const pkgimage::package_entry& expected,
                         const pkgstate::owned_entry& actual)
{
  const std::string& path = expected.path.string();
  const pkgstate::installed_object_metadata& object = actual.object();

  if (actual.kind() != state_kind(expected.type))
    state_mismatch(package, path, "kind");
  if (object.mode() != expected.mode)
    state_mismatch(package, path, "mode");
  if (object.uid() != expected.uid)
    state_mismatch(package, path, "uid");
  if (object.gid() != expected.gid)
    state_mismatch(package, path, "gid");
  if (object.mtime().seconds() != expected.mtime ||
      object.mtime().nanoseconds() != expected.mtime_nanoseconds)
  {
    state_mismatch(package, path, "mtime");
  }
  if (actual.rejected())
    state_mismatch(package, path, "unexpected rejected-object evidence");

  switch (expected.type)
  {
  case pkgimage::entry_type::regular:
    if (!expected.regular_content || !object.size() ||
        *object.size() != expected.size || !object.regular_content() ||
        object.regular_content()->string() != expected.regular_content->string())
    {
      state_mismatch(package, path, "regular content");
    }
    if (object.hardlink_anchor())
      state_mismatch(package, path, "unexpected hard-link anchor");
    break;

  case pkgimage::entry_type::hardlink:
  {
    if (!expected.hardlink_target || !object.hardlink_anchor() ||
        object.hardlink_anchor()->string() != expected.hardlink_target->string())
    {
      state_mismatch(package, path, "hard-link anchor");
    }
    const pkgimage::package_entry* anchor = image.find(*expected.hardlink_target);
    if (anchor == nullptr || anchor->type != pkgimage::entry_type::regular ||
        !anchor->regular_content || !object.size() ||
        *object.size() != anchor->size || !object.regular_content() ||
        object.regular_content()->string() != anchor->regular_content->string())
    {
      state_mismatch(package, path, "hard-link content");
    }
    break;
  }

  case pkgimage::entry_type::directory:
  case pkgimage::entry_type::fifo:
    if (object.size() || object.regular_content() || object.symlink_target() ||
        object.device() || object.hardlink_anchor())
    {
      state_mismatch(package, path, "inapplicable metadata");
    }
    break;

  case pkgimage::entry_type::symlink:
    if (!expected.symlink_target || !object.symlink_target() ||
        *object.symlink_target() != *expected.symlink_target)
    {
      state_mismatch(package, path, "symlink target");
    }
    break;

  case pkgimage::entry_type::character_device:
  case pkgimage::entry_type::block_device:
    if (!expected.device || !object.device() ||
        object.device()->major() != expected.device->major ||
        object.device()->minor() != expected.device->minor)
    {
      state_mismatch(package, path, "device number");
    }
    break;
  }
}

void require_state_package(const pkgstate::snapshot& state,
                           const package_authority& authority)
{
  const pkgstate::installed_package* installed =
      state.find_package(authority.name);
  if (installed == nullptr)
    throw std::runtime_error(authority.name + ": expected installed package is absent");

  const pkgimage::package_image& image = authority.inspection.image();
  if (image.size() == 0U)
    throw std::runtime_error(authority.name + ": sealed package image is empty");
  if (installed->manifest().empty())
    throw std::runtime_error(authority.name + ": installed manifest is empty");
  if (installed->manifest().size() != image.size())
  {
    throw std::runtime_error(
        authority.name + ": installed manifest size differs from package image");
  }

  for (const pkgimage::package_entry& expected : image.entries())
  {
    const pkgstate::package_path path =
        pkgstate::package_path::parse(expected.path.string());
    const pkgstate::owned_entry* actual = installed->find(path);
    if (actual == nullptr)
      state_mismatch(authority.name, expected.path.string(), "missing state entry");
    require_state_entry(authority.name, image, expected, *actual);
  }

  for (const pkgstate::owned_entry& actual : installed->manifest())
  {
    if (image.find(pkgimage::package_path::parse(actual.path().string())) == nullptr)
      state_mismatch(authority.name, actual.path().string(), "state-only path");
  }
}

[[nodiscard]] package_authority inspect_authority(const char* name,
                                                  const char* scope,
                                                  const char* archive,
                                                  const char* digest,
                                                  const char* marker_path,
                                                  const char* marker_line)
{
  if (*name == '\0')
    throw std::invalid_argument("package authority name is empty");

  bool installed = false;
  if (std::string_view(scope) == "installed")
    installed = true;
  else if (std::string_view(scope) != "build-only")
    throw std::invalid_argument("package authority scope is not installed/build-only");

  pkgimage::archive_inspection_request request;
  request.source = archive;
  request.expected_archive_digest =
      pkgimage::complete_archive_digest::parse(archive_digest(digest));
  pkgimage::inspected_package_image inspection =
      pkgimage::libarchive_backend().inspect(request);

  const pkgimage::package_entry* marker = inspection.image().find(
      pkgimage::package_path::parse(marker_path));
  if (marker == nullptr)
    throw std::runtime_error(std::string(name) + ": package image lacks marker");
  if (marker->type != pkgimage::entry_type::regular)
    throw std::runtime_error(std::string(name) + ": marker is not regular");

  const std::string expected_payload = std::string(marker_line) + '\n';
  const auto selection =
      pkgimage::entry_selection::from_ids(inspection.image(), {marker->id});
  auto replay = pkgimage::libarchive_backend().open(request);
  collecting_sink sink(marker->id);
  replay->replay(selection, sink);
  if (!sink.complete() || sink.payload() != expected_payload)
    throw std::runtime_error(std::string(name) + ": marker payload differs from authority");

  return package_authority{std::string(name), installed, std::move(inspection)};
}

[[nodiscard]] std::string_view finding_name(pkgaudit::finding_kind kind)
{
  switch (kind)
  {
  case pkgaudit::finding_kind::missing_object:
    return "missing-object";
  case pkgaudit::finding_kind::object_class_mismatch:
    return "object-class-mismatch";
  case pkgaudit::finding_kind::dangling_symlink:
    return "dangling-symlink";
  case pkgaudit::finding_kind::symlink_loop:
    return "symlink-loop";
  case pkgaudit::finding_kind::symlink_target_outside_root:
    return "symlink-target-outside-root";
  }
  return "unknown";
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 9 || (argc - 3) % 6 != 0)
    return 2;

  try
  {
    const auto store = pkgstate::posix::canonical_generation_store::open_existing(
        argv[1], test_support::binding());
    const pkgstate::snapshot state = store.read();

    std::vector<package_authority> authorities;
    for (int index = 3; index < argc; index += 6)
    {
      authorities.push_back(inspect_authority(
          argv[index], argv[index + 1], argv[index + 2], argv[index + 3],
          argv[index + 4], argv[index + 5]));
    }
    std::sort(authorities.begin(), authorities.end(),
              [](const package_authority& lhs, const package_authority& rhs) {
                return lhs.name < rhs.name;
              });
    for (std::size_t index = 1; index < authorities.size(); ++index)
    {
      if (authorities[index - 1].name == authorities[index].name)
        throw std::runtime_error("duplicate package authority: " + authorities[index].name);
    }

    std::vector<pkgaudit::package_facts> audit_packages;
    std::size_t expected_installed = 0U;
    std::size_t expected_objects = 0U;
    for (const package_authority& authority : authorities)
    {
      const pkgstate::installed_package* installed =
          state.find_package(authority.name);
      if (!authority.installed)
      {
        if (installed != nullptr)
          throw std::runtime_error(authority.name + ": build-only package entered state");
        continue;
      }

      ++expected_installed;
      require_state_package(state, authority);

      std::vector<pkgaudit::expected_object> objects;
      objects.reserve(authority.inspection.image().size());
      for (const pkgimage::package_entry& entry : authority.inspection.image().entries())
      {
        objects.push_back({
            pkgaudit::object_path::parse(entry.path.string()),
            audit_kind(entry.type),
        });
      }
      expected_objects += objects.size();
      audit_packages.emplace_back(authority.name, std::move(objects));
    }

    if (expected_installed == 0U || expected_objects == 0U)
      throw std::runtime_error("independent installed image authority is vacuous");
    if (state.packages().size() != expected_installed)
      throw std::runtime_error("canonical state package set differs from image authority");

    const pkgaudit::inventory facts(std::move(audit_packages));
    auto filesystem = pkgaudit::make_posix_filesystem_backend({argv[2], 40});
    pkgaudit::audit_request request;
    request.packages = pkgaudit::package_selection::all();
    request.checks = pkgaudit::check_set({
        pkgaudit::check::object_state,
        pkgaudit::check::symlink_resolution,
        pkgaudit::check::symlink_ownership,
    });
    const pkgaudit::report report =
        pkgaudit::auditor().run(facts, request, *filesystem);

    std::cout << "complete " << (report.complete() ? "yes" : "no") << '\n'
              << "artifacts " << authorities.size() << '\n'
              << "packages " << expected_installed << '\n'
              << "objects " << expected_objects << '\n'
              << "findings " << report.findings().size() << '\n'
              << "relations " << report.relations().size() << '\n'
              << "failures " << report.failures().size() << '\n';
    for (const auto& finding : report.findings())
      std::cout << "finding " << finding_name(finding.kind) << ' '
                << finding.package << ' ' << finding.path.string() << '\n';

    if (!report.complete())
      return 2;
    return report.findings().empty() ? EXIT_SUCCESS : 1;
  }
  catch (const std::exception& error)
  {
    std::cerr << "rootfs-authority-audit-fixture: " << error.what() << '\n';
    return 2;
  }
}
