#include "phantomledger/app/backend.hpp"
#include "phantomledger/app/cli.hpp"
#include "phantomledger/app/options.hpp"
#include "phantomledger/app/orchestrator.hpp"
#include "phantomledger/app/progress.hpp"
#include "phantomledger/app/setup.hpp"
#include "phantomledger/synth/econ/catalog.hpp"

#include <exception>
#include <print>

int main(int argc, char **argv) {
  using namespace ::PhantomLedger;
  namespace pii = synth::pii;
  namespace pg = app::progress;

  try {
    const auto opts = app::cli::parse(argc, argv);

    // Era Notice
    const auto &era = synth::econ::macroSeries();
    if (auto notice =
            app::frozenEraNotice(opts, era.firstYear(), era.lastYear())) {
      std::println(stderr, "{}", *notice);
    }

    auto backendResult = app::backend::resolve(opts);
    if (!backendResult.has_value()) {
      std::println(stderr, "fatal: {}", backendResult.error());
      return 1;
    }
    const auto &backend = backendResult.value();

    time::Window window;
    window.start = time::makeTime(opts.startDate);
    window.days = static_cast<int>(opts.days);

    pg::status("Building entity synthesis config...");
    const auto mix = pii::LocaleMix::usBankDefault();
    const auto pools = app::setup::buildPoolSet(opts, mix);
    const auto entityConfig =
        app::setup::buildEntitySynthesis(opts, pools, mix, window.start);

    app::orchestrator::StreamOrchestrator orchestrator(opts, window, pools,
                                                       entityConfig, backend);

    return orchestrator.run();

  } catch (const std::exception &e) {
    std::println(stderr, "fatal: {}", e.what());
    return 1;
  } catch (...) {
    std::println(stderr, "fatal: unknown exception");
    return 1;
  }
}
