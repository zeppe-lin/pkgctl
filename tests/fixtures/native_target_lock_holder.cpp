// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/identity.h>

#include <libpkgapply-posix/mutation_lease.h>
#include <libpkgapply/target_context.h>
#include <libpkgplan/digest.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

class fd_guard final {
public:
  explicit fd_guard(int value = -1) noexcept : value_(value) {}
  fd_guard(const fd_guard&) = delete;
  fd_guard& operator=(const fd_guard&) = delete;
  ~fd_guard()
  {
    if (value_ >= 0)
      static_cast<void>(::close(value_));
  }
  [[nodiscard]] int get() const noexcept { return value_; }

private:
  int value_;
};

void request_stop(int) noexcept
{
  stop_requested = 1;
}

template<typename Identity>
[[nodiscard]] Identity digest(char marker)
{
  return Identity::parse(
      "v1:sha256:" + std::string(64U, marker));
}

[[nodiscard]] pkgapply::mutation_exclusion_domain_identity mutation_domain(
    std::string_view transaction,
    std::string_view target_binding,
    const std::filesystem::path& target_root,
    const std::filesystem::path& runtime_root,
    const std::filesystem::path& lock_root)
{
  const std::vector<std::string> fields{
      "mutation-domain",
      std::string(transaction),
      std::string(target_binding),
      target_root.string(),
      runtime_root.string(),
      "libpkgapply-posix/3.1",
      lock_root.string(),
  };
  return pkgapply::mutation_exclusion_domain_identity::parse(
      "v1:sha256:" +
      pkgctl::make_session_identity(
          "pkgctl/native-command-mutation-domain/1", fields)
          .hex());
}

[[nodiscard]] pkgapply::application_target_context target_context(
    const pkgapply::mutation_exclusion_domain_identity& domain)
{
  return pkgapply::application_target_context::make(
      pkgplan::target_system_context_identity::parse(
          "v1:sha256:" + std::string(64U, '0')),
      digest<pkgapply::managed_target_identity>('1'),
      digest<pkgapply::root_view_identity>('2'),
      digest<pkgapply::observation_backend_identity>('3'),
      digest<pkgapply::mutation_backend_identity>('4'),
      domain,
      digest<pkgapply::active_object_namespace_identity>('5'),
      digest<pkgapply::rejected_object_store_identity>('6'),
      digest<pkgapply::staging_namespace_identity>('7'),
      digest<pkgapply::journal_namespace_identity>('8'),
      digest<pkgapply::execution_capability_profile_identity>('9'));
}

[[nodiscard]] std::filesystem::path normalized_absolute(
    const char* value,
    std::string_view role)
{
  std::filesystem::path path(value);
  if (!path.is_absolute() || path.empty() || path != path.lexically_normal())
    throw std::invalid_argument(
        std::string(role) + " must be an absolute normalized path");
  return path;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 6)
  {
    std::cerr << "usage: native-target-lock-holder LOCK_ROOT TRANSACTION "
                 "TARGET_BINDING TARGET_ROOT RUNTIME_ROOT\n";
    return EXIT_FAILURE;
  }

  try
  {
    const auto lock_root = normalized_absolute(argv[1], "lock root");
    const auto transaction = pkgctl::session_identity::from_hex(argv[2]);
    const std::string target_binding(argv[3]);
    const auto target_root = normalized_absolute(argv[4], "target root");
    const auto runtime_root = normalized_absolute(argv[5], "runtime root");
    if (lock_root != runtime_root / "target-locks")
      throw std::invalid_argument(
          "lock root is not the runtime target-locks authority");

    fd_guard directory_fd(::open(
        lock_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (directory_fd.get() < 0)
      throw std::runtime_error(
          "cannot open target-lock directory: " +
          std::string(std::strerror(errno)));

    const auto domain = mutation_domain(
        transaction.hex(), target_binding, target_root, runtime_root,
        lock_root);
    auto target = target_context(domain);
    auto lease = pkgapply::posix::target_mutation_lease::acquire(
        target, directory_fd.get());

    struct sigaction action {};
    action.sa_handler = request_stop;
    static_cast<void>(::sigemptyset(&action.sa_mask));
    action.sa_flags = 0;
    if (::sigaction(SIGTERM, &action, nullptr) != 0 ||
        ::sigaction(SIGINT, &action, nullptr) != 0 ||
        ::sigaction(SIGHUP, &action, nullptr) != 0)
      throw std::runtime_error("cannot install holder signal handler");

    std::cout << "ready " << domain.string() << '\n' << std::flush;
    while (!stop_requested)
      static_cast<void>(::pause());

    return lease->held() ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  catch (const std::exception& problem)
  {
    std::cerr << "native-target-lock-holder: " << problem.what() << '\n';
    return EXIT_FAILURE;
  }
}
