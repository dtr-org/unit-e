// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <settings.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

struct Fixture {

  std::unique_ptr<::ArgsManager> args_manager;
  std::unique_ptr<blockchain::Behavior> behavior;
  std::unique_ptr<::Settings> settings;

  Fixture(std::initializer_list<std::string> args)
      : args_manager([&] {
          std::unique_ptr<::ArgsManager> argsman = MakeUnique<::ArgsManager>();
          const char **argv = new const char *[args.size() + 1];
          argv[0] = "executable-name";
          std::size_t i = 1;
          for (const auto &arg : args) {
            argv[i++] = arg.c_str();
          }
          argsman->ParseParameters(static_cast<int>(i), argv);
          delete[] argv;
          return argsman;
        }()),
        behavior(blockchain::Behavior::New(args_manager.get())),
        settings(Settings::New(args_manager.get(), behavior.get())) {}
};

BOOST_FIXTURE_TEST_SUITE(settings_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(regtest_not_proposing_by_default) {

  Fixture f1{"-proposing=0", "-regtest"};
  BOOST_CHECK(!f1.settings->node_is_proposer);

  Fixture f2{"-proposing=1", "-regtest"};
  BOOST_CHECK(f2.settings->node_is_proposer);

  Fixture f3{"-regtest"};
  BOOST_CHECK(!f3.settings->node_is_proposer);
}

BOOST_AUTO_TEST_SUITE_END()
