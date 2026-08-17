// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgimage/entry_selection.h>
#include <libpkgimage/libarchive_backend.h>
#include <libpkgimage/package_path.h>
#include <libpkgimage/payload_sink.h>

#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view marker_path = "usr/lib/shared-ownership-marker";
constexpr std::string_view marker_payload = "shared-authority\n";
constexpr std::string_view marker_digest =
    "v1:sha256:6e231219a7f7e1cef0e59ba1184018819a47b4bebafcd5c9d257e0f6f11d2a06";

class collecting_sink final : public pkgimage::payload_sink {
public:
  void begin(const pkgimage::package_entry& entry) override
  {
    if (began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected marker replay begin");
    began_ = true;
  }

  void write(const pkgimage::package_entry& entry,
             const std::byte* data,
             std::size_t size) override
  {
    if (!began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected marker replay write");
    payload_.append(reinterpret_cast<const char*>(data), size);
  }

  void end(const pkgimage::package_entry& entry) override
  {
    if (!began_ || ended_ || entry.id != expected_)
      throw std::runtime_error("unexpected marker replay end");
    ended_ = true;
  }

  explicit collecting_sink(pkgimage::entry_id expected) : expected_(expected)
  {
  }

  [[nodiscard]] const std::string& payload() const noexcept
  {
    return payload_;
  }

  [[nodiscard]] bool complete() const noexcept
  {
    return began_ && ended_;
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

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
    return 2;

  try
  {
    pkgimage::archive_inspection_request request;
    request.source = argv[1];
    request.expected_archive_digest =
        pkgimage::complete_archive_digest::parse(archive_digest(argv[2]));

    auto archive = pkgimage::libarchive_backend().open(request);
    const pkgimage::package_entry* marker =
        archive->image().find(pkgimage::package_path::parse(marker_path));
    if (marker == nullptr)
      throw std::runtime_error("sealed package image lacks shared marker");
    if (marker->type != pkgimage::entry_type::regular)
      throw std::runtime_error("shared marker is not a regular file");
    if (marker->mode != 0644U)
      throw std::runtime_error("shared marker mode differs from 0644");
    if (marker->size != marker_payload.size())
      throw std::runtime_error("shared marker size differs from fixture authority");
    if (!marker->regular_content || marker->regular_content->string() != marker_digest)
      throw std::runtime_error("shared marker content identity differs from fixture authority");

    const auto selection =
        pkgimage::entry_selection::from_ids(archive->image(), {marker->id});
    collecting_sink sink(marker->id);
    archive->replay(selection, sink);
    if (!sink.complete() || sink.payload() != marker_payload)
      throw std::runtime_error("shared marker replay bytes differ from fixture authority");

    const auto& receipt = archive->inspection_receipt();
    std::cout << "archive " << receipt.archive_digest().string() << '\n'
              << "image " << archive->image().identity().string() << '\n'
              << "path " << marker->path.string() << '\n'
              << "type regular\n"
              << "mode " << std::oct << std::setw(4) << std::setfill('0')
              << marker->mode << std::dec << '\n'
              << "uid " << marker->uid << '\n'
              << "gid " << marker->gid << '\n'
              << "size " << marker->size << '\n'
              << "mtime " << marker->mtime << '\n'
              << "mtime-nanoseconds " << marker->mtime_nanoseconds << '\n'
              << "content " << marker->regular_content->string() << '\n'
              << "payload shared-authority\\n\n";
    return EXIT_SUCCESS;
  }
  catch (const std::exception& error)
  {
    std::cerr << "shared-ownership-image-fixture: " << error.what() << '\n';
    return 2;
  }
}
