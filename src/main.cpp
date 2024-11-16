#include <chrono>
#include <unordered_map>

#include "io.h"
#include "util.h"

int main(int argc, char *argv[]) {
  const auto startTotal = std::chrono::high_resolution_clock::now();

  // 处理命令行参数
  const auto &[input_dir, output_file] = util::parse_cmd_options(argc, argv);

  Graph finest;
  FPGA fpgas;
  IOSystem io_system;
  std::unordered_map<std::string, int> fpga_map, node_map;
  std::unordered_map<int, std::string> fpga_reverse_map, node_reverse_map;
  io_system.read_input_dir(
      input_dir, finest, fpgas, fpga_map, node_map, fpga_reverse_map,
      node_reverse_map);

  std::vector<int> parts(finest.nodes.size()); // 划分结果

  // 输出
  io_system.write_design_fpga_out(
      output_file, parts, fpga_reverse_map, node_reverse_map);

  // 运行性能信息统计输出
  util::printPeakMem();
  const auto endTotal = std::chrono::high_resolution_clock::now();
  util::printTime(startTotal, endTotal);

  return 0;
}
