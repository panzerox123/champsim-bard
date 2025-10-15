/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include <fmt/core.h>

#include "cache.h" // for CACHE
#include "champsim.h"
#ifndef CHAMPSIM_TEST_BUILD
#include "core_inst.inc"
#endif
#include "defaults.hpp"
#include "dram_channel.h"
#include "environment.h"
#include "ooo_cpu.h" // for O3_CPU
#include "phase_info.h"
#include "stats_printer.h"
#include "tracereader.h"
#include "vmem.h"

namespace champsim
{
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces);
}

/*
 * EXTERNAL OPTIONS:
 * */
bool             OPT_CACHE_ENABLE_VWQ;
bool             OPT_CACHE_ENABLE_EAGER_WRITEBACK;

DRAM_PAGE_POLICY OPT_DRAM_PAGE_POLICY;
bool             OPT_DRAM_IDEAL_WLP;
bool             OPT_DRAM_USE_X8_WRITE_TIMING;

bool OPT_BARD_USE_ROW_BUFFER_HITS;
bool OPT_BARD_USE_BITVECTOR;

bool OPT_BARD_ONLY_PROACTIVE_WRITEBACK;
bool OPT_BARD_ONLY_SHADOW_WRITEBACK;

int OPT_BARD_MAX_LOOKUP;
int OPT_BARD_SAMPLED_SETS;

bool OPT_BARD_USE_UTILITY_COUNTERS;

#ifndef CHAMPSIM_TEST_BUILD
using configured_environment = champsim::configured::generated_environment<CHAMPSIM_BUILD>;

const std::size_t NUM_CPUS = configured_environment::num_cpus;

const unsigned BLOCK_SIZE = configured_environment::block_size;
const unsigned PAGE_SIZE = configured_environment::page_size;

#endif

const unsigned LOG2_BLOCK_SIZE = champsim::lg2(BLOCK_SIZE);
const unsigned LOG2_PAGE_SIZE = champsim::lg2(PAGE_SIZE);

#ifndef CHAMPSIM_TEST_BUILD
int main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
  configured_environment gen_environment{};

  CLI::App app{"A microarchitecture simulator for research and education"};

  bool knob_cloudsuite{false};
  long long warmup_instructions = 0;
  long long simulation_instructions = std::numeric_limits<long long>::max();
  std::string json_file_name;
  std::vector<std::string> trace_names;

  auto set_heartbeat_callback = [&](auto) {
    for (O3_CPU& cpu : gen_environment.cpu_view()) {
      cpu.show_heartbeat = false;
    }
  };

  app.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app.add_flag("--hide-heartbeat", set_heartbeat_callback, "Hide the heartbeat output");
  auto* warmup_instr_option = app.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  auto* deprec_warmup_instr_option =
      app.add_option("--warmup_instructions", warmup_instructions, "[deprecated] use --warmup-instructions instead")->excludes(warmup_instr_option);
  auto* sim_instr_option = app.add_option("-i,--simulation-instructions", simulation_instructions,
                                          "The number of instructions in the detailed phase. If not specified, run to the end of the trace.");
  auto* deprec_sim_instr_option =
      app.add_option("--simulation_instructions", simulation_instructions, "[deprecated] use --simulation-instructions instead")->excludes(sim_instr_option);

  auto* json_option =
      app.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")->expected(0, 1);

  app.add_option("traces", trace_names, "The paths to the traces")->required()->expected(NUM_CPUS)->check(CLI::ExistingFile);
  /*
   * Additional params and flags::
   * */
  int dram_page_policy = 0;

  app.add_flag("--cache-enable-vwq", OPT_CACHE_ENABLE_VWQ, "enable VWQ for caches with a non-null DRAM ptr");
  app.add_flag("--cache-enable-eager-writeback", OPT_CACHE_ENABLE_EAGER_WRITEBACK, "enable Eager Writeback for caches with a non-null DRAM ptr");

  app.add_option("--dram-page-policy", dram_page_policy, "0 = open, 1 = close, 2 = soft-close");
  app.add_flag("--dram-ideal-wlp", OPT_DRAM_IDEAL_WLP, "enable to ensure writes go to banks uniformly");
  app.add_flag("--dram-use-x8-write-timing", OPT_DRAM_USE_X8_WRITE_TIMING, "enable to set tCCD_L_WR = 10ns");
  /*
   * BARD OPTIONS;
   * */
  OPT_BARD_MAX_LOOKUP = -1;
  OPT_BARD_SAMPLED_SETS = 64;

  app.add_flag("--bard-use-row-buffer-hits", OPT_BARD_USE_ROW_BUFFER_HITS, "enable so BARD tries to maintain RBHR");
  app.add_flag("--bard-use-bitvector", OPT_BARD_USE_BITVECTOR, "enable so BARD uses one bit per bank");

  app.add_flag("--bard-only-proactive-writeback", OPT_BARD_ONLY_PROACTIVE_WRITEBACK, "enable to disable BARD's shadow writeback");
  app.add_flag("--bard-only-shadow-writeback", OPT_BARD_ONLY_SHADOW_WRITEBACK, "enable to disable BARD's shadow writeback");

  app.add_option("--bard-max-lookup", OPT_BARD_MAX_LOOKUP, "max way lookup for bard (default = -1, which enables use mark-recapture)");
  app.add_option("--bard-sampled-sets", OPT_BARD_SAMPLED_SETS, "sampled sets for bard (default = 64)");

  app.add_flag("--bard-use-utility-counters", OPT_BARD_USE_UTILITY_COUNTERS, "set to use utility counters instead of mark recapture");

  CLI11_PARSE(app, argc, argv);

  const bool warmup_given = (warmup_instr_option->count() > 0) || (deprec_warmup_instr_option->count() > 0);
  const bool simulation_given = (sim_instr_option->count() > 0) || (deprec_sim_instr_option->count() > 0);

  if (deprec_warmup_instr_option->count() > 0) {
    fmt::print("WARNING: option --warmup_instructions is deprecated. Use --warmup-instructions instead.\n");
  }

  if (deprec_sim_instr_option->count() > 0) {
    fmt::print("WARNING: option --simulation_instructions is deprecated. Use --simulation-instructions instead.\n");
  }

  if (simulation_given && !warmup_given) {
    // Warmup is 20% by default
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    warmup_instructions = simulation_instructions / 5;
  }

  /*
   * Additional setup:
   * */
  OPT_DRAM_PAGE_POLICY = static_cast<DRAM_PAGE_POLICY>(dram_page_policy);

  std::vector<champsim::tracereader> traces;
  std::transform(
      std::begin(trace_names), std::end(trace_names), std::back_inserter(traces),
      [knob_cloudsuite, repeat = simulation_given, i = uint8_t(0)](auto name) mutable { return get_tracereader(name, i++, knob_cloudsuite, repeat); });

  std::vector<champsim::phase_info> phases{
      {champsim::phase_info{"Warmup", true, warmup_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names},
       champsim::phase_info{"Simulation", false, simulation_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names}}};

  for (auto& p : phases) {
    std::iota(std::begin(p.trace_index), std::end(p.trace_index), 0);
  }

  fmt::print("\n*** ChampSim Multicore Out-of-Order Simulator ***\nWarmup Instructions: {}\nSimulation Instructions: {}\nNumber of CPUs: {}\nPage size: {}\n\n",
             phases.at(0).length, phases.at(1).length, std::size(gen_environment.cpu_view()), PAGE_SIZE);

  auto phase_stats = champsim::main(gen_environment, phases, traces);

  fmt::print("\nChampSim completed all CPUs\n\n");

  champsim::plain_printer{std::cout}.print(phase_stats);

  for (CACHE& cache : gen_environment.cache_view()) {
    cache.impl_prefetcher_final_stats();
  }

  for (CACHE& cache : gen_environment.cache_view()) {
    cache.impl_replacement_final_stats();
  }

  if (json_option->count() > 0) {
    if (json_file_name.empty()) {
      champsim::json_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream json_file{json_file_name};
      champsim::json_printer{json_file}.print(phase_stats);
    }
  }

  return 0;
}
#endif
