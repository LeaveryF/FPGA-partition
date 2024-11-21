#pragma once

#include <getopt.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Sparse>

#include "libmtkahypar.h"
#include "libmtkahypartypes.h"

// 结点
class Node {
public:
  // old: 点权定义为所有资源的代数和
  // new: 点权定义为占用比例最大的资源的比例乘以总资源
  int weight = 0; // 点权
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

  int pin_size = 0; // 引脚数
  Eigen::VectorXi required_res; // 总耗费资源
};

// 所有fpga
class FPGA {
public:
  int size = 0; // fpga个数
  std::vector<Eigen::VectorXi> resources; // 资源
  std::vector<std::vector<int>> topology; // 拓扑 // 取值0/1 // 邻接矩阵

  int num_edges = 0; // 边数
  std::vector<int> edges; // 目标图的边 // for mt lib

  Eigen::VectorXi total_res; // 总资源
  Eigen::VectorXi lower_res; // 资源下界

  std::vector<int> max_dist; // 每个fpga到其他fpga的最大距离 // 仅用于求s_hat
  std::vector<std::vector<int>> dist; // 距离矩阵
  std::vector<std::vector<std::set<int>>>
      s_hat; // 可达结点集合 s_hat[i][x]表示与节点i距离不大于x的结点集合

  int max_hops; // 最大跳数
  int max_interconnects; // 最大对外互连数(该fpga的割边权重)
};

class Utils {
public:
  // used by main.cpp

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

  static void print_peak_mem() {
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

  static void print_time(
      const std::chrono::_V2::system_clock::time_point &start,
      const std::chrono::_V2::system_clock::time_point &end,
      const std::string &str) {
    std::cout << str << " time: " << std::fixed << std::setprecision(3)
              << std::chrono::duration_cast<std::chrono::nanoseconds>(
                     end - start)
                         .count() *
                     1e-9
              << " s" << std::endl;
  }

  // used by MtPartitioner
  static void construct_mt_hypergraph(
      const Graph &finest, mt_kahypar_hypergraph_t &hypergraph) {
    // 超边索引数组
    std::unique_ptr<size_t[]> hyperedge_indices =
        std::make_unique<size_t[]>(finest.nets.size() + 1);
    size_t cnt = 0;
    for (int i = 0; i < finest.nets.size(); i++) {
      hyperedge_indices[i] = cnt;
      cnt += finest.nets[i].nodes.size();
    }
    hyperedge_indices[finest.nets.size()] = cnt;
    // 超边数组
    std::unique_ptr<mt_kahypar_hyperedge_id_t[]> hyperedges =
        std::make_unique<mt_kahypar_hyperedge_id_t[]>(finest.pin_size);
    int index = 0;
    for (const auto &net : finest.nets) {
      for (const auto &node : net.nodes) {
        hyperedges[index++] = node;
      }
    }
    // 节点权重数组
    std::unique_ptr<mt_kahypar_hypernode_weight_t[]> node_weights =
        std::make_unique<mt_kahypar_hypernode_weight_t[]>(finest.nodes.size());
    for (int i = 0; i < finest.nodes.size(); i++) {
      node_weights[i] = finest.nodes[i].weight;
    }
    // 超边权重数组
    std::unique_ptr<mt_kahypar_hyperedge_weight_t[]> hyperedge_weights =
        std::make_unique<mt_kahypar_hyperedge_weight_t[]>(finest.nets.size());
    for (int i = 0; i < finest.nets.size(); i++) {
      hyperedge_weights[i] = finest.nets[i].weight;
    }
    // Construct hypergraph
    hypergraph = mt_kahypar_create_hypergraph(
        DETERMINISTIC, finest.nodes.size(), finest.nets.size(),
        hyperedge_indices.get(), hyperedges.get(), hyperedge_weights.get(),
        node_weights.get());
  }

  static void construct_mt_target_graph(
      const FPGA &fpgas, mt_kahypar_target_graph_t **target_grapt) {
    // 边数组
    std::unique_ptr<mt_kahypar_hypernode_id_t[]> edges =
        std::make_unique<mt_kahypar_hypernode_id_t[]>(fpgas.edges.size());
    for (int i = 0; i < fpgas.edges.size(); i++) {
      edges[i] = fpgas.edges[i];
    }
    // 边权数组
    std::unique_ptr<mt_kahypar_hyperedge_weight_t[]> edge_weights =
        std::make_unique<mt_kahypar_hyperedge_weight_t[]>(fpgas.num_edges);
    for (int i = 0; i < fpgas.num_edges; i++) {
      edge_weights[i] = 1;
    }
    // Construct target graph
    *target_grapt = mt_kahypar_create_target_graph(
        fpgas.size, fpgas.num_edges, edges.get(), edge_weights.get());
  }

  static void get_mt_partition_results(
      const mt_kahypar_partitioned_hypergraph_t &partitioned_hg,
      std::vector<int> &parts) {
    std::unique_ptr<mt_kahypar_partition_id_t[]> mapping =
        std::make_unique<mt_kahypar_partition_id_t[]>(parts.size());
    mt_kahypar_get_partition(partitioned_hg, mapping.get());
    for (int i = 0; i < parts.size(); i++) {
      parts[i] = mapping[i];
    }
  }

  // used by ...

  static void get_fpgas_assignments(
      const std::vector<int> parts,
      std::vector<std::vector<int>> &fpgas_assignments) {
    for (int i = 0; i < parts.size(); i++) {
      if (parts[i] < 0 || parts[i] >= fpgas_assignments.size()) {
        std::cerr << "Node " << i << "'s part invaild. Partition failed."
                  << std::endl;
        exit(1);
      }
      fpgas_assignments[parts[i]].push_back(i);
    }
  }

  static bool check_all_fpgas_resources(
      const std::vector<Eigen::VectorXi> &fpgas_resources,
      const std::vector<Node> &nodes, const std::vector<int> &parts) {
    std::vector<std::vector<int>> fpgas_assignments(fpgas_resources.size());
    get_fpgas_assignments(parts, fpgas_assignments);
    for (int i = 0; i < fpgas_assignments.size(); i++) {
      if (!Utils::check_single_fpga_resource(
              fpgas_resources[i], fpgas_assignments[i], nodes)) {
        return false;
      }
    }
    return true;
  }

  static bool check_single_fpga_resource(
      const Eigen::VectorXi &resources,
      const std::vector<int> &fpgas_assignment,
      const std::vector<Node> &nodes) {
    Eigen::VectorXi total_res = Eigen::VectorXi::Zero(resources.size());
    for (const auto &x : fpgas_assignment) {
      total_res += nodes[x].resources;
      if ((total_res - resources).minCoeff() < 0) {
        return false;
      }
    }
    return true;
  }

  static int get_total_hop_length(
      const std::vector<Net> &nets, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist, const int max_hop,
      int &violation) {
    int total_hop_length = 0;
    for (const auto &net : nets) {
      int hop_count = get_single_hop_count(net, parts, dist);
      if (hop_count == -1) {
        continue;
      }
      if (hop_count > max_hop) {
        violation++;
      }
      total_hop_length += hop_count * net.weight;
    }
    return total_hop_length;
  }

  static int get_single_hop_count(
      const Net &net, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist) {
    int src_fpga = parts[net.nodes[0]];
    std::set<int> involved_fpgas;
    for (auto &node : net.nodes) {
      involved_fpgas.insert(parts[node]);
    }
    if (involved_fpgas.size() == 1) {
      return -1;
    }
    involved_fpgas.erase(src_fpga);
    int hop_count = 0;
    for (const auto &dst_fpga : involved_fpgas) {
      hop_count += dist[src_fpga][dst_fpga];
    }
    return hop_count;
  }
};
