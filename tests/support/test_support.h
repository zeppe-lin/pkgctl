// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libpkgstate-posix/canonical_generation_store.h>

#include <pkgctl/request.h>

namespace test_support {

inline void replace_with_fifo(const std::filesystem::path& path)
{
  if (!std::filesystem::remove(path))
    throw std::runtime_error("cannot remove retained-authority fixture file: " +
                             path.string());
  if (::mkfifo(path.c_str(), 0444) != 0)
    throw std::runtime_error("cannot create retained-authority FIFO: " +
                             path.string());
}

template<typename Function>
bool child_reports_without_blocking(Function&& function)
{
  const pid_t child = ::fork();
  if (child < 0)
    throw std::runtime_error("cannot fork nonblocking-refusal fixture");
  if (child == 0)
  {
    ::alarm(2U);
    bool accepted = false;
    try
    {
      accepted = function();
    }
    catch (...)
    {
      accepted = false;
    }
    ::_exit(accepted ? 0 : 1);
  }

  int status = 0;
  if (::waitpid(child, &status, 0) != child)
    throw std::runtime_error("cannot wait for nonblocking-refusal fixture");
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

class temporary_directory final {
public:
  temporary_directory()
  {
    std::array<char, 64> pattern{};
    const std::string value =
        (std::filesystem::temp_directory_path() / "pkgctl-test.XXXXXX").string();
    if (value.size() + 1U > pattern.size())
      throw std::runtime_error("temporary pathname is too long");
    std::copy(value.begin(), value.end(), pattern.begin());
    char* created = ::mkdtemp(pattern.data());
    if (!created)
      throw std::runtime_error("cannot create temporary directory");
    path_ = created;
  }
  temporary_directory(const temporary_directory&) = delete;
  temporary_directory& operator=(const temporary_directory&) = delete;
  ~temporary_directory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  [[nodiscard]] const std::filesystem::path& path() const noexcept
  { return path_; }
private:
  std::filesystem::path path_;
};

inline void write(const std::filesystem::path& path, const std::string& text)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("cannot create fixture file: " + path.string());
  output << text;
  if (!output)
    throw std::runtime_error("cannot write fixture file: " + path.string());
}

inline std::string recipe(const std::string& name,
                          const std::string& requirements)
{
  return "format: zeppe-lin.recipe/1\n"
         "\n"
         "package:\n"
         "  name: " + name + "\n"
         "  version: 1.0\n"
         "  release: 1\n"
         "  summary: " + name + " package\n"
         "  licenses:\n"
         "    - GPL-3.0-or-later\n"
         "\n"
         "requirements:\n" + requirements +
         "\n"
         "sources: []\n"
         "\n"
         "build:\n"
         "  language: posix-shell\n"
         "  script: |\n"
         "    true\n"
         "\n"
         "architectures:\n"
         "  build:\n"
         "    - x86_64\n"
         "  target:\n"
         "    - x86_64\n";
}

inline void create_collection(const std::filesystem::path& root)
{
  write(root / "profiles.yml",
        "format: zeppe-lin.profiles/1\n"
        "\n"
        "profiles:\n"
        "  base:\n"
        "    members:\n"
        "      - package: app\n");
  write(root / "app" / "recipe.yml",
        recipe("app",
               "  build:\n"
               "    - package: tool\n"
               "  run:\n"
               "    - package: libfoo\n"));
  write(root / "libfoo" / "recipe.yml", recipe("libfoo", "  {}\n"));
  write(root / "tool" / "recipe.yml", recipe("tool", "  {}\n"));
}

template<typename Identity>
Identity identity(unsigned char marker)
{
  pkgstate::sha256_digest_bytes bytes{};
  bytes.back() = marker;
  return Identity::from_sha256(bytes);
}

inline pkgstate::state_target_binding binding()
{
  return pkgstate::state_target_binding::make(
      identity<pkgstate::managed_target_identity>(1),
      identity<pkgstate::state_store_identity>(2),
      identity<pkgstate::root_view_identity>(3),
      identity<pkgstate::state_backend_identity>(4),
      identity<pkgstate::publication_domain_identity>(5));
}

inline void initialize_state(const std::filesystem::path& root)
{
  const pkgstate::posix::canonical_generation_store store(root, binding());
  (void)store;
}

inline pkgctl::catalog_request catalog_request(
    const std::filesystem::path& root,
    std::string name = "core")
{
  std::vector<pkgcatalog::acquire::collection_specification> collections;
  collections.emplace_back(
      0, pkgcatalog::collection_reference(std::move(name)), root,
      std::nullopt,
      pkgsource::declaration_provenance("<test>", "collections[0]", 1, 1));
  return pkgctl::catalog_request::make(std::move(collections));
}

inline pkgctl::resolution_request resolution_request(
    const std::filesystem::path& collection_root,
    const std::filesystem::path& state_root,
    pkgresolve::installed_preference preference =
        pkgresolve::installed_preference::retain_compatible)
{
  std::vector<pkgresolve::resolution_goal> goals;
  goals.emplace_back(
      pkgsource::requirement_scope::build(),
      pkgsource::requirement_subject(pkgsource::profile_reference("@base")),
      "<test-build>");
  goals.emplace_back(
      pkgsource::requirement_scope::run(),
      pkgsource::requirement_subject(pkgsource::profile_reference("@base")),
      "<test-run>");
  return pkgctl::resolution_request::make(
      catalog_request(collection_root),
      pkgctl::state_location::make(state_root, binding()),
      pkgresolve::architecture_context(
          pkgsource::architecture_reference("x86_64"),
          pkgsource::architecture_reference("x86_64")),
      std::move(goals), pkgresolve::resolution_policy(preference));
}

} // namespace test_support
