#include <cstdint>
#include <filesystem>
#include <format>
#include <random>
#include <string_view>

#include <gtest/gtest.h>

#include "../src/options.hpp"
#include "../src/tags.hpp"
#include "../src/log.hpp"
#include "../src/test_runner.hpp"
#include "../src/parser.hpp"
#include "../src/test_results.hpp"
#include "../src/cucumber.hpp"

#include "test_paths.hpp"

TEST(options, file_path_doesnt_exist)
{
  const char* argv[] = {"program", "path/doesnt/exist/to/file.feature"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_TRUE(prog_args.get_feature_files().empty());
  ASSERT_TRUE(prog_args.get_excluded_files().empty());
}
TEST(options, file_path_does_exist)
{
  std::string path =
      std::format("{}/test_files/any.feature", unittests::test_dir());
  const char* argv[] = {"program", path.c_str()};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_FALSE(prog_args.get_feature_files().empty());
  EXPECT_EQ(prog_args.get_feature_files().at(0).path, std::string(argv[1]));
}

namespace details
{
[[nodiscard]] bool has_file(
    const std::vector<cuke::internal::feature_file>& container,
    std::string_view file_name)
{
  for (const cuke::internal::feature_file& file : container)
  {
    if (file.path.ends_with(file_name))
    {
      return true;
    }
  }
  return false;
}
}  // namespace details

TEST(options, find_files_in_dir)
{
  std::string path = std::format("{}/test_files", unittests::test_dir());
  const char* argv[] = {"program", path.c_str()};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_EQ(prog_args.get_feature_files().size(), 4);

  EXPECT_TRUE(details::has_file(prog_args.get_feature_files(), "any.feature"));
  EXPECT_TRUE(
      details::has_file(prog_args.get_feature_files(), "example.feature"));
  EXPECT_TRUE(details::has_file(prog_args.get_feature_files(), "fail.feature"));
  EXPECT_TRUE(details::has_file(prog_args.get_feature_files(), "skip.feature"));
}
namespace details
{
static std::string remove_trailing_char(std::string_view str, std::size_t n)
{
  return std::string(str.substr(0, str.size() - n));
}
}  // namespace details
TEST(options, file_path_does_exist_w_line)
{
  std::string path =
      std::format("{}/test_files/any.feature:3", unittests::test_dir());
  const char* argv[] = {"program", path.c_str()};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_FALSE(prog_args.get_feature_files().empty());
  EXPECT_EQ(prog_args.get_feature_files().at(0).path,
            details::remove_trailing_char(argv[1], 2));
  ASSERT_FALSE(prog_args.get_feature_files().at(0).lines_to_run.empty());
  EXPECT_TRUE(prog_args.get_feature_files().at(0).lines_to_run.contains(3));
}
TEST(options, file_path_does_exist_w_lines)
{
  std::string path = std::format("{}/test_files/any.feature:3:123:9999",
                                 unittests::test_dir());
  const char* argv[] = {"program", path.c_str()};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_FALSE(prog_args.get_feature_files().empty());
  EXPECT_EQ(prog_args.get_feature_files().at(0).path,
            details::remove_trailing_char(argv[1], 11));
  ASSERT_EQ(prog_args.get_feature_files().at(0).lines_to_run.size(), 3);
  EXPECT_TRUE(prog_args.get_feature_files().at(0).lines_to_run.contains(3));
  EXPECT_TRUE(prog_args.get_feature_files().at(0).lines_to_run.contains(123));
  EXPECT_TRUE(prog_args.get_feature_files().at(0).lines_to_run.contains(9999));
}
TEST(options, tag_expression_1)
{
  const char* argv[] = {"program", "-t", "@tag1 or @tag2"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_TRUE(prog_args.is_set(cuke::internal::program_args::arg::tags));
  ASSERT_FALSE(
      prog_args.get_value(cuke::internal::program_args::arg::tags).empty());

  cuke::internal::tag_expression tags(
      prog_args.get_value(cuke::internal::program_args::arg::tags));
  EXPECT_TRUE(tags.evaluate(std::vector{std::string{"@tag1"}}));
  EXPECT_TRUE(tags.evaluate(std::vector{std::string{"@tag2"}}));
  EXPECT_FALSE(tags.evaluate(std::vector{std::string{"@tag3"}}));
}
TEST(options, tag_expression_2)
{
  const char* argv[] = {"program", "--tags", "@tag1 or @tag2"};
  int argc = sizeof(argv) / sizeof(argv[0]);
  cuke::internal::program_args prog_args;
  prog_args.initialize(argc, argv);
  ASSERT_TRUE(prog_args.is_set(cuke::internal::program_args::arg::tags));
  ASSERT_FALSE(
      prog_args.get_value(cuke::internal::program_args::arg::tags).empty());

  cuke::internal::tag_expression tags(
      prog_args.get_value(cuke::internal::program_args::arg::tags));

  EXPECT_TRUE(tags.evaluate(std::vector{std::string{"@tag1"}}));
  EXPECT_TRUE(tags.evaluate(std::vector{std::string{"@tag2"}}));
  EXPECT_FALSE(tags.evaluate(std::vector{std::string{"@tag3"}}));
}

// A report written to a file leaves stdout free, so the run stays visible.
// A report written to stdout must not be mixed with log lines.
class report_json_logging : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    cuke::registry().clear();
    cuke::results::test_results().clear();
    cuke::registry().push_step(cuke::internal::step_definition(
        [](const cuke::value_array&, const auto&, const auto&, const auto&) {},
        "a step"));
  }
  void TearDown() override
  {
    cuke::internal::get_program_args(0, {}).clear();
    cuke::log::enable();
    // Belt and suspenders: report_json_path() already lives outside the
    // repository, but a test that writes it should still not depend on the
    // next test (or the next run) to clean up after it.
    std::error_code ec;
    std::filesystem::remove(report_json_path(), ec);
  }

  // Under the OS temp directory rather than a bare relative name: portable
  // (no POSIX-only assumption, works on the windows-latest job too) and
  // never lands inside the repository working tree, so running the suite
  // never leaves a stray file for `git status` to notice.
  //
  // The name itself is randomized once per process (a mkstemp-equivalent,
  // in portable standard C++ rather than a platform header): a fixed name
  // in a shared temp directory would collide between two unittests runs on
  // the same machine, or fail these assertions outright if some other
  // user's leftover file with that name is already there and not ours to
  // remove or overwrite. Every test in this fixture reuses the same
  // process-wide name, so TearDown() always targets the exact file a test
  // wrote.
  static std::string report_json_path()
  {
    static const std::string unique_name = []
    {
      std::random_device rd;
      std::mt19937_64 gen(rd());
      std::uniform_int_distribution<std::uint64_t> dist;
      return std::format("cwt-cucumber-report-{:016x}.json", dist(gen));
    }();
    return (std::filesystem::temp_directory_path() / unique_name).string();
  }

  static std::string run_with(int argc, const char* argv[])
  {
    [[maybe_unused]] auto& args = cuke::internal::get_program_args(argc, argv);
    testing::internal::CaptureStdout();
    const char* script = R"*(
      Feature: a feature
      Scenario: a scenario
      Given a step
    )*";
    cuke::parser p;
    p.parse_script(script);
    cuke::test_runner runner;
    p.for_each_scenario(runner);
    return testing::internal::GetCapturedStdout();
  }
};

TEST_F(report_json_logging, no_flags_keeps_the_run_visible)
{
  const char* argv[] = {"cucumber"};
  const std::string out = run_with(1, argv);
  EXPECT_NE(out.find("a scenario"), std::string::npos);
}

TEST_F(report_json_logging, a_named_file_keeps_the_run_visible)
{
  const std::string path = report_json_path();
  const char* argv[] = {"cucumber", "--report-json", path.c_str()};
  const std::string out = run_with(3, argv);
  EXPECT_NE(out.find("a scenario"), std::string::npos);
}

TEST_F(report_json_logging, a_report_on_stdout_stays_silent)
{
  const char* argv[] = {"cucumber", "--report-json"};
  const std::string out = run_with(2, argv);
  EXPECT_EQ(out.find("a scenario"), std::string::npos);
}

// print_results() is what actually decides whether the run summary
// (Failed Scenarios:, then the scenario/step counts) reaches the terminal.
// A named file must get both the JSON file and that summary; a bare
// --report-json must keep writing only JSON to stdout.
TEST_F(report_json_logging, print_results_w_named_file_also_prints_the_summary)
{
  const std::string path = report_json_path();
  const char* argv[] = {"cucumber", "--report-json", path.c_str()};
  cuke::cwt_cucumber cucumber(3, argv);

  const char* script = R"*(
      Feature: a feature
      Scenario: a scenario
      Given a step
    )*";
  cuke::parser p;
  p.parse_script(script);
  cuke::test_runner runner;
  p.for_each_scenario(runner);

  testing::internal::CaptureStdout();
  cucumber.print_results();
  const std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("1 Scenario ("), std::string::npos);
  EXPECT_NE(out.find("1 Step ("), std::string::npos);

  // print_results() must also have actually written the JSON file, not
  // just printed the terminal summary.
  ASSERT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 0u);
}

TEST_F(report_json_logging,
       print_results_w_no_flags_prints_full_run_and_summary)
{
  const char* argv[] = {"cucumber"};
  cuke::cwt_cucumber cucumber(1, argv);

  const char* script = R"*(
      Feature: a feature
      Scenario: a scenario
      Given a step
    )*";
  cuke::parser p;
  p.parse_script(script);
  cuke::test_runner runner;
  p.for_each_scenario(runner);

  testing::internal::CaptureStdout();
  cucumber.print_results();
  const std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("1 Scenario ("), std::string::npos);
  EXPECT_NE(out.find("1 Step ("), std::string::npos);
}

TEST_F(report_json_logging, print_results_w_bare_option_prints_json_only)
{
  const char* argv[] = {"cucumber", "--report-json"};
  cuke::cwt_cucumber cucumber(2, argv);

  const char* script = R"*(
      Feature: a feature
      Scenario: a scenario
      Given a step
    )*";
  cuke::parser p;
  p.parse_script(script);
  cuke::test_runner runner;
  p.for_each_scenario(runner);

  testing::internal::CaptureStdout();
  cucumber.print_results();
  const std::string out = testing::internal::GetCapturedStdout();

  EXPECT_EQ(out.find("1 Scenario ("), std::string::npos);
  EXPECT_EQ(out.find("1 Step ("), std::string::npos);
#ifdef WITH_JSON
  EXPECT_NE(out.find("\"elements\""), std::string::npos);
#endif  // WITH_JSON
}
