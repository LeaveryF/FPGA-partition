#include <chrono>
#include <unordered_map>

#include "io.h"
#include "partition.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  const auto startTotal = std::chrono::high_resolution_clock::now();

  // 处理命令行参数
  const auto &[input_dir, output_file] = Utils::parse_cmd_options(argc, argv);

  // 输入
  Graph finest;
  FPGA fpgas;
  std::unordered_map<std::string, int> fpga_map, node_map;
  std::unordered_map<int, std::string> fpga_reverse_map, node_reverse_map;

  IO::read_input_dir(
      input_dir, finest, fpgas, fpga_map, node_map, fpga_reverse_map,
      node_reverse_map);

  // 划分
  std::vector<int> parts(finest.nodes.size()); // 划分结果

  Partition::partition(finest, fpgas, parts);

  // 输出
  IO::write_design_fpga_out(
      output_file, parts, fpga_reverse_map, node_reverse_map);

  // 运行性能信息统计输出
  Utils::print_peak_mem();
  const auto endTotal = std::chrono::high_resolution_clock::now();
  Utils::print_time(startTotal, endTotal, "Total");
  std::cout << std::endl;

  return 0;
}
