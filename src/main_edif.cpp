#include <chrono>
#include <getopt.h>
#include <vector>

#include "io.h"
#include "partition.h"

using namespace std;

bool inoutExist = false; // TODO: to be delete
vector<string> workLibVec;
vector<string> workCellVec;

int main(int argc, char *argv[]) {
  const auto startTotal = chrono::high_resolution_clock::now();
  spdlog::set_pattern("[%^%l%$] %v");
  spdlog::set_level(spdlog::level::info); // ! change when finally submit

  // 处理命令行参数
  static struct option long_options[] = {
      {"input-dir", required_argument, 0, 't'},
      {"output-file", required_argument, 0, 's'},
      {0, 0, 0, 0}};
  std::string input_dir, output_file;
  int opt, index = 0;
  while ((opt = getopt_long(argc, argv, "t:s:", long_options, &index)) != -1) {
    switch (opt) {
    case 't':
      input_dir = optarg;
      break;
    case 's':
      output_file = optarg;
      break;
    default:
      std::cerr << "Usage: " << argv[0] << " -t <input_dir> -s <output-file>"
                << std::endl;
      return 1;
    }
  }
  // 检查必需的参数是否都已提供
  if (input_dir == "" || output_file == "") {
    std::cerr << "Usage: " << argv[0] << " -t <input_dir> -s <output-file>"
              << std::endl;
    return 1;
  }

  // 输入
  params para;
  graph finest;
  fpga fpgas;
  flat_hash_map<string, int> node_map;
  flat_hash_map<string, int> fpga_map;
  flat_hash_map<int, string> node_reverse_map;
  flat_hash_map<int, string> fpga_reverse_map;

  IO::IOSystem io_system(
      input_dir, finest, fpgas, fpga_map, node_map, fpga_reverse_map,
      node_reverse_map);
  para.max_hop = fpgas.maxHops; // just avoid passing para
  // evil
  if (finest.nodes.size() >= 1000000) {
    para.hold_solution_num = 1;
    para.coarsening_ratio = 10;
  }
  if (fpgas.dist.size() <= 4) {
    para.isLogicalRepetition = true;
  }
  para.isLogicalRepetition = false; // ! disable now
  spdlog::info("Finish reading inputs.");

  // 划分
  // 包含coarsen initial refine
  spdlog::info("Start Partition.");
  resultVariable fpgaResult;
  partition(finest, fpgas, fpgaResult.parts, para);
  spdlog::info("Finish Partition.");

  // todo: refactor this
  if (para.isLogicalRepetition) {
    Refinement refinement_obj(
        para.has_fix, para.has_io, para.has_timing, para.force_topo,
        para.max_move, para.max_neg_move, para.refine_iters,
        para.num_best_initial_solutions, para.thread, para.max_hop, para._k,
        para.enable_Greedy, para.enable_HER, para.enable_EA, para.enable_PM,
        fpgas.cutweights_assignment, fpgas.dist, fpgas.bound_constraint,
        fpgas.upper_resources - fpgas.lower_resources);
    refinement_obj.refinePartition(fpgaResult.parts, fpgas.resources, finest);
    // cout << "Logical Replication Finished." << endl;
  }

  auto cutweights = partUtil::checkResults(finest, fpgas, fpgaResult.parts);
  // if (para.debug_mode) {
  //   output_cut_result(finest, fpgaResult.parts);
  // }

  // 输出
  io_system.write_design_fpga_out(
      finest, output_file, fpgaResult.parts, fpga_reverse_map,
      node_reverse_map);

  tbb::global_control gc(
      tbb::global_control::max_allowed_parallelism, para.thread);

  // if (para.clockMode == 1 || fpgaResult.VccGndPartFlag)
  //   result::writeCuts(fpgaResult.cutweights, fpgaResult.cutweight);
  // if (fpgas.cutweights_assignment.size() > 0) {
  //   result::checkCutWeights(fpgaResult.cutweights,
  //   fpgas.cutweights_assignment);
  // }

  // 运行性能信息统计输出
  runtimeMsg::printPeakMem();
  const auto endTotal = chrono::high_resolution_clock::now();
  runtimeMsg::printTime(startTotal, endTotal, "all");

  return 0;
}
