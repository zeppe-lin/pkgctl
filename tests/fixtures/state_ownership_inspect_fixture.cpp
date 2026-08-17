// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/test_support.h"

#include <libpkgstate-posix/canonical_generation_store.h>
#include <libpkgstate/owned_entry.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const char* kind_name(pkgstate::owned_object_kind kind)
{
  switch (kind)
  {
    case pkgstate::owned_object_kind::regular:
      return "regular";
    case pkgstate::owned_object_kind::directory:
      return "directory";
    case pkgstate::owned_object_kind::symlink:
      return "symlink";
    case pkgstate::owned_object_kind::fifo:
      return "fifo";
    case pkgstate::owned_object_kind::character_device:
      return "character-device";
    case pkgstate::owned_object_kind::block_device:
      return "block-device";
    case pkgstate::owned_object_kind::socket:
      return "socket";
    case pkgstate::owned_object_kind::other:
      return "other";
  }
  throw std::runtime_error("unknown installed object kind");
}

const char* origin_name(pkgstate::active_object_origin origin)
{
  switch (origin)
  {
    case pkgstate::active_object_origin::incoming_payload:
      return "incoming-payload";
    case pkgstate::active_object_origin::retained_existing:
      return "retained-existing";
  }
  throw std::runtime_error("unknown active object origin");
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 4)
  {
    std::cerr << "usage: state-ownership-inspect-fixture STORE PACKAGE PATH\n";
    return 2;
  }

  try
  {
    const auto store = pkgstate::posix::canonical_generation_store::open_existing(
        argv[1], test_support::binding());
    const auto state = store.read();
    const auto* package = state.find_package(argv[2]);
    if (package == nullptr)
      throw std::runtime_error("selected installed package is absent");

    const auto path = pkgstate::package_path::parse(argv[3]);
    const auto* entry = package->find(path);
    if (entry == nullptr)
      throw std::runtime_error("selected package does not own requested path");

    const auto owners = state.owners(path);
    const auto& object = entry->object();
    const auto& receipt = package->receipt();

    std::cout << "snapshot " << state.identity().string() << '\n'
              << "ownership " << state.ownership_identity().string() << '\n'
              << "packages " << state.size() << '\n'
              << "package " << package->release().name() << ' '
              << package->release().version_release() << '\n'
              << "package-identity " << package->identity().string() << '\n'
              << "manifest " << package->manifest().size() << '\n'
              << "path " << entry->path().string() << '\n'
              << "kind " << kind_name(entry->kind()) << '\n'
              << "origin " << origin_name(entry->origin()) << '\n'
              << "mode " << std::setfill('0') << std::setw(4) << std::oct
              << object.mode() << std::dec << '\n'
              << "uid " << object.uid() << '\n'
              << "gid " << object.gid() << '\n'
              << "mtime " << object.mtime().seconds() << '\n'
              << "mtime-nanoseconds " << object.mtime().nanoseconds() << '\n';
    if (object.size())
      std::cout << "size " << *object.size() << '\n';
    if (object.regular_content())
      std::cout << "content " << object.regular_content()->string() << '\n';
    if (object.symlink_target())
      std::cout << "symlink-target " << *object.symlink_target() << '\n';
    if (object.hardlink_anchor())
      std::cout << "hardlink-anchor " << object.hardlink_anchor()->string()
                << '\n';

    std::cout << "owners " << owners.size() << '\n';
    for (std::size_t index = 0U; index < owners.size(); ++index)
    {
      std::cout << "owner." << index << ' ' << owners[index]->release().name()
                << ' ' << owners[index]->identity().string() << '\n';
    }

    std::cout << "operation-plan " << receipt.operation_plan().string() << '\n'
              << "application-evidence "
              << receipt.application_evidence().string() << '\n';
    if (receipt.transaction_evidence())
      std::cout << "transaction-evidence "
                << receipt.transaction_evidence()->string() << '\n';
  }
  catch (const std::exception& error)
  {
    std::cerr << "state-ownership-inspect-fixture: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
