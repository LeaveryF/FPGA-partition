#pragma once

#include <getopt.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Sparse>

// 结点
class Node {
public:
  int weight; // 点权
  Eigen::VectorXi resources; // 8种资源
};

// 超边
class Net {
public:
  int weight; // 边权
  std::vector<int> nodes; // 节点 // 0 is src
};

// 超图
class Graph {
public:
  std::vector<Node> nodes; // 节点
  std::vector<Net> nets; // 超边
  std::vector<std::vector<int>> incident_edges; // 每个结点关联的所有超边
};

// 所有fpga
class FPGA {
public:
  int size; // fpga个数
  std::vector<Eigen::VectorXi> resources; // 资源
  std::vector<std::vector<int>> topology; // 拓扑 // 取值0/1

  std::vector<int> max_dist; // 每个fpga到其他fpga的最大距离 // 仅用于求s_hat
  std::vector<std::vector<int>> dist; // 距离矩阵
  std::vector<std::vector<std::set<int>>>
      s_hat; // 可达结点集合 s_hat[i][x]表示与节点i距离不大于x的结点集合

  int max_hops; // 最大跳数
  int max_interconnects; // 最大对外互连数(该fpga的割边权重)
};

class Util {
public:
  static std::pair<std::string, std::string>
  parse_cmd_options(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"input-dir", required_argument, 0, 't'},
        {"output-file", required_argument, 0, 's'},
        {0, 0, 0, 0}};
    std::string input_dir, output_file;
    int opt, index = 0;
    while ((opt = getopt_long(argc, argv, "t:s:", long_options, &index)) !=
           -1) {
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
        exit(1);
      }
    }
    // 检查必需的参数是否都已提供
    if (input_dir == "" || output_file == "") {
      std::cerr << "Usage: " << argv[0] << " -t <input_dir> -s <output-file>"
                << std::endl;
      exit(1);
    }

    return {input_dir, output_file};
  }

  static void printPeakMem() {
    // /proc/self/status 关键字段说明
    // Name:     com.sample.name   // 进程名
    // FDSize:   800               // 当前进程申请的文件句柄个数
    // VmPeak:   3004628 kB        // 当前进程的虚拟内存峰值大小
    // VmSize:   2997032 kB        // 当前进程的虚拟内存大小
    // Threads:  600               // 当前进程包含的线程个数

    std::ifstream file("/proc/self/status");
    std::string word, vm_peak;
    while (file >> word) {
      if (word == "VmPeak:") {
        file >> vm_peak;
        break;
      }
    }
    file.close();

    int index = vm_peak.size() - 1;
    for (int i = index - 2; i >= 0; i -= 3) {
      if (i > 0) {
        vm_peak.insert(i, 1, ',');
      }
    }

    std::cout << "Peak memory: " << vm_peak << " KB" << std::endl;
  }

  static void printTime(
      const std::chrono::_V2::system_clock::time_point &start,
      const std::chrono::_V2::system_clock::time_point &end) {
    std::cout << "Total time: " << std::fixed << std::setprecision(3)
              << std::chrono::duration_cast<std::chrono::nanoseconds>(
                     end - start)
                         .count() *
                     1e-9
              << " s" << std::endl;
  }
};
