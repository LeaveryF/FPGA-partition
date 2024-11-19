#include <chrono>
#include <unordered_map>

#include "io.h"
#include "partition.h"
#include "util.h"

int main(int argc, char *argv[]) {
  const auto startTotal = std::chrono::high_resolution_clock::now();

  // 处理命令行参数
  const auto &[input_dir, output_file] = Util::parse_cmd_options(argc, argv);

  // 输入
  Graph finest;
  FPGA fpgas;
  IOSystem io_system;
  std::unordered_map<std::string, int> fpga_map, node_map;
  std::unordered_map<int, std::string> fpga_reverse_map, node_reverse_map;

  io_system.read_input_dir(
      input_dir, finest, fpgas, fpga_map, node_map, fpga_reverse_map,
      node_reverse_map);

  // 划分
  std::vector<int> parts(finest.nodes.size()); // 划分结果

  Partitioner partitioner;
  partitioner.partition(finest, fpgas, parts);

  // 输出
  io_system.write_design_fpga_out(
      output_file, parts, fpga_reverse_map, node_reverse_map);

  // 运行性能信息统计输出
  Util::printPeakMem();
  const auto endTotal = std::chrono::high_resolution_clock::now();
  Util::printTime(startTotal, endTotal);
  std::cout << std::endl;

  return 0;
}
