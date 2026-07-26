// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <pkgctl/pkgctl.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void
check(bool condition, const char* expression, int line)
{
  if (!condition)
  {
    std::cerr << "line " << line << ": check failed: " << expression << '\n';
    ++failures;
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

template<typename Function>
void
expect_error(pkgctl::error_code expected, Function&& function, int line)
{
  try
  {
    function();
    std::cerr << "line " << line << ": expected pkgctl::error\n";
    ++failures;
  }
  catch (const pkgctl::error& caught)
  {
    if (caught.code() != expected)
    {
      std::cerr << "line " << line << ": wrong pkgctl::error code\n";
      ++failures;
    }
  }
}

#define EXPECT_ERROR(code, expression) \
  expect_error((code), [&] { (void)(expression); }, __LINE__)

pkgctl::package_name
package(const char* name)
{
  return pkgctl::package_name::parse(name);
}

pkgctl::operation_id
operation(const char* id)
{
  return pkgctl::operation_id::parse(id);
}

void
test_package_names()
{
  CHECK(package("libarchive").string() == "libarchive");
  CHECK(package("xorg-server").string() == "xorg-server");
  EXPECT_ERROR(pkgctl::error_code::invalid_package_name,
               pkgctl::package_name::parse(""));
  EXPECT_ERROR(pkgctl::error_code::invalid_package_name,
               pkgctl::package_name::parse("bad/name"));
  EXPECT_ERROR(pkgctl::error_code::invalid_package_name,
               pkgctl::package_name::parse("bad name"));
}

void
test_intents()
{
  const auto install = pkgctl::install_intent::make(
      {package("zlib"), package("acl")});
  CHECK(install.targets().size() == 2);
  CHECK(install.targets()[0].string() == "acl");
  CHECK(install.targets()[1].string() == "zlib");

  const pkgctl::user_intent wrapped = install;
  CHECK(pkgctl::kind(wrapped) == pkgctl::intent_kind::install);
  CHECK(pkgctl::target_packages(wrapped).size() == 2);

  const pkgctl::user_intent sysup = pkgctl::system_update_intent{};
  CHECK(pkgctl::kind(sysup) == pkgctl::intent_kind::system_update);
  CHECK(pkgctl::target_packages(sysup).empty());

  EXPECT_ERROR(pkgctl::error_code::invalid_intent,
               pkgctl::remove_intent::make({}));
  EXPECT_ERROR(pkgctl::error_code::invalid_intent,
               pkgctl::download_intent::make(
                   {package("curl"), package("curl")}));
}

void
test_constraints()
{
  auto constraints = pkgctl::constraint_set::make({
      pkgctl::require_candidate(package("openssl")),
      pkgctl::exclude_target(package("openssl")),
      pkgctl::forbid_node(package("bash")),
  });

  CHECK(constraints.constraints().size() == 3);
  CHECK(pkgctl::kind(constraints.constraints()[0]) ==
        pkgctl::constraint_kind::exclude_target);
  CHECK(pkgctl::kind(constraints.constraints()[1]) ==
        pkgctl::constraint_kind::forbid_node);
  CHECK(pkgctl::kind(constraints.constraints()[2]) ==
        pkgctl::constraint_kind::require_candidate);
  CHECK(pkgctl::constrained_package(constraints.constraints()[0]).string() ==
        "openssl");
  CHECK(pkgctl::constrained_package(constraints.constraints()[2]).string() ==
        "openssl");

  EXPECT_ERROR(pkgctl::error_code::invalid_constraint,
               pkgctl::constraint_set::make({
                   pkgctl::prune_subtree(package("llvm")),
                   pkgctl::prune_subtree(package("llvm")),
               }));
}

void
test_outcomes()
{
  const auto success = pkgctl::step_outcome::succeeded(
      pkgctl::step_kind::planning);
  CHECK(success.state() == pkgctl::outcome_state::succeeded);
  CHECK(!success.failure());
  CHECK(!success.skip());
  CHECK(success.diagnostic().empty());

  const auto refusal = pkgctl::step_outcome::refused(
      pkgctl::step_kind::planning,
      pkgctl::failure_domain::policy,
      "candidate is forbidden by transaction policy");
  CHECK(refusal.state() == pkgctl::outcome_state::refused);
  CHECK(refusal.failure() == pkgctl::failure_domain::policy);
  CHECK(!refusal.skip());

  const auto failure = pkgctl::step_outcome::failed(
      pkgctl::step_kind::build,
      pkgctl::failure_domain::external_process,
      "build authority failed");
  CHECK(failure.state() == pkgctl::outcome_state::failed);
  CHECK(failure.failure() == pkgctl::failure_domain::external_process);

  const auto skipped = pkgctl::step_outcome::skipped(
      pkgctl::step_kind::application,
      pkgctl::skip_reason::prerequisite_failed,
      "required build did not complete");
  CHECK(skipped.state() == pkgctl::outcome_state::skipped);
  CHECK(!skipped.failure());
  CHECK(skipped.skip() == pkgctl::skip_reason::prerequisite_failed);

  EXPECT_ERROR(pkgctl::error_code::invalid_outcome,
               pkgctl::step_outcome::failed(
                   pkgctl::step_kind::build,
                   pkgctl::failure_domain::external_process,
                   ""));

  CHECK(pkgctl::to_string(pkgctl::step_kind::state_publication) ==
        "state-publication");
  CHECK(pkgctl::to_string(pkgctl::failure_domain::stale_authority) ==
        "stale-authority");
}

pkgctl::package_operation
make_operation(const char* id,
               const char* package_name,
               pkgctl::operation_kind kind,
               std::vector<pkgctl::operation_id> prerequisites = {})
{
  return pkgctl::package_operation::make(
      operation(id), package(package_name), kind, std::move(prerequisites));
}

void
test_operation_graph()
{
  const auto graph = pkgctl::operation_graph::make({
      make_operation("install-b", "b", pkgctl::operation_kind::install,
                     {operation("upgrade-a")}),
      make_operation("upgrade-a", "a", pkgctl::operation_kind::upgrade),
      make_operation("install-c", "c", pkgctl::operation_kind::install,
                     {operation("install-b")}),
  });

  CHECK(graph.operations().size() == 3);
  CHECK(graph.operations()[0].id().string() == "install-b");
  CHECK(graph.operations()[1].id().string() == "install-c");
  CHECK(graph.operations()[2].id().string() == "upgrade-a");

  CHECK(graph.execution_order().size() == 3);
  CHECK(graph.execution_order()[0].string() == "upgrade-a");
  CHECK(graph.execution_order()[1].string() == "install-b");
  CHECK(graph.execution_order()[2].string() == "install-c");

  const auto* found = graph.find(operation("install-b"));
  CHECK(found != nullptr);
  CHECK(found->package().string() == "b");
  CHECK(graph.find(operation("absent")) == nullptr);

  const auto reordered = pkgctl::operation_graph::make({
      make_operation("install-c", "c", pkgctl::operation_kind::install,
                     {operation("install-b")}),
      make_operation("upgrade-a", "a", pkgctl::operation_kind::upgrade),
      make_operation("install-b", "b", pkgctl::operation_kind::install,
                     {operation("upgrade-a")}),
  });
  CHECK(reordered.execution_order() == graph.execution_order());

  const auto independent = pkgctl::operation_graph::make({
      make_operation("z", "z", pkgctl::operation_kind::install),
      make_operation("a", "a", pkgctl::operation_kind::install),
  });
  CHECK(independent.execution_order()[0].string() == "a");
  CHECK(independent.execution_order()[1].string() == "z");

  const auto empty = pkgctl::operation_graph::make({});
  CHECK(empty.operations().empty());
  CHECK(empty.execution_order().empty());

  EXPECT_ERROR(pkgctl::error_code::invalid_operation,
               pkgctl::package_operation::make(
                   operation("self"), package("self"),
                   pkgctl::operation_kind::install,
                   {operation("self")}));

  EXPECT_ERROR(pkgctl::error_code::missing_prerequisite,
               pkgctl::operation_graph::make({
                   make_operation("a", "a", pkgctl::operation_kind::install,
                                  {operation("missing")}),
               }));

  EXPECT_ERROR(pkgctl::error_code::duplicate_operation,
               pkgctl::operation_graph::make({
                   make_operation("same", "a", pkgctl::operation_kind::install),
                   make_operation("same", "b", pkgctl::operation_kind::install),
               }));

  EXPECT_ERROR(pkgctl::error_code::duplicate_operation,
               pkgctl::operation_graph::make({
                   make_operation("one", "a", pkgctl::operation_kind::install),
                   make_operation("two", "a", pkgctl::operation_kind::upgrade),
               }));

  EXPECT_ERROR(pkgctl::error_code::cyclic_operation_graph,
               pkgctl::operation_graph::make({
                   make_operation("a", "a", pkgctl::operation_kind::install,
                                  {operation("b")}),
                   make_operation("b", "b", pkgctl::operation_kind::install,
                                  {operation("a")}),
               }));
}

} // namespace

int
main()
{
  test_package_names();
  test_intents();
  test_constraints();
  test_outcomes();
  test_operation_graph();
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
