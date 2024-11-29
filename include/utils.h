#pragma once

#include <getopt.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Sparse>

#include "libmtkahypar.h"
#include "libmtkahypartypes.h"

// 节点
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
  std::vector<std::vector<int>> incident_edges; // 每个节点关联的所有超边

  int pin_size = 0; // 引脚数
  Eigen::VectorXi required_res; // 总耗费资源
};

// 所有fpga
class FPGA {
public:
  int size = 0; // fpga个数
  std::vector<Eigen::VectorXi> resources; // 资源
  std::vector<std::vector<int>> topology; // 拓扑 // 取值0/1 // 邻接矩阵

  int num_edges = 0; // 有向边数 // = 无向边数*2
  std::vector<int> edges; // 目标图的边 // for mt lib // size = 2*num_edges

  Eigen::VectorXi total_res; // 总资源
  Eigen::VectorXi lower_res; // 资源下界

  std::vector<int> max_dist; // 每个fpga到其他fpga的最大距离 // 仅用于求s_hat
  std::vector<std::vector<int>> dist; // 距离矩阵
  std::vector<std::vector<std::set<int>>>
      s_hat; // 可达节点集合 s_hat[i][x]表示与节点i距离不大于x的节点集合

  int max_hops; // 最大跳数
  int max_interconnects; // 最大对外互连数(该fpga的割边权重)
};

class Utils {
public:
  // used by main.cpp

  static void init_program() {
    std::cout << std::fixed << std::setprecision(2);
  }

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
      const std::string &info_str) {
    std::cout << info_str << " time: " << std::fixed << std::setprecision(3)
              << std::chrono::duration_cast<std::chrono::nanoseconds>(
                     end - start)
                         .count() *
                     1e-9
              << " s" << std::endl;
  }

  // used by MtPartitioner mt_partition_lib
  static void construct_mt_hypergraph(
      const Graph &finest, mt_kahypar_hypergraph_t &hypergraph,
      const mt_kahypar_preset_type_t &type) {
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
        type, finest.nodes.size(), finest.nets.size(), hyperedge_indices.get(),
        hyperedges.get(), hyperedge_weights.get(), node_weights.get());
  }

  static void construct_mt_target_graph(
      const FPGA &fpgas, mt_kahypar_target_graph_t **target_grapt) {
    // 边数组
    std::unique_ptr<mt_kahypar_hypernode_id_t[]> edges =
        std::make_unique<mt_kahypar_hypernode_id_t[]>(fpgas.num_edges * 2);
    for (int i = 0; i < fpgas.num_edges * 2; i++) {
      edges[i] = fpgas.edges[i];
    }
    // Construct target graph
    *target_grapt = mt_kahypar_create_target_graph(
        fpgas.size, fpgas.num_edges, edges.get(), nullptr);
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

  // 获取划分后的分组结果

  static void get_assignments(
      const std::vector<int> parts,
      std::vector<std::unordered_set<int>> &assignments) {
    for (int i = 0; i < parts.size(); i++) {
      if (parts[i] < 0 || parts[i] >= assignments.size()) {
        std::cerr << "Node " << i << "'s part invaild. Partition failed."
                  << std::endl;
        exit(1);
      }
      assignments[parts[i]].insert(i);
    }
  }

  static void get_required_res(
      const std::vector<std::unordered_set<int>> &assignments,
      const std::vector<Node> &nodes,
      std::vector<Eigen::VectorXi> &required_res) {
    for (int i = 0; i < assignments.size(); i++) {
      Eigen::VectorXi res = Eigen::VectorXi::Zero(8);
      for (const auto &x : assignments[i]) {
        res += nodes[x].resources;
      }
      required_res.push_back(res);
    }
  }

  // 输出资源信息

  static void print_res_mat(
      const std::string &info_str_1, const std::string &info_str_2,
      const std::vector<Eigen::VectorXi> &res) {
    std::cout << info_str_1 << ": " << std::endl;
    for (int i = 0; i < res.size(); i++) {
      std::cout << info_str_2 << ' ' << i << ": ";
      for (int j = 0; j < 8; j++) {
        std::cout << res[i][j] << ' ';
      }
      std::cout << std::endl;
    }
  }

  static void
  print_res_vec(const std::string &info_str, const Eigen::VectorXi res) {
    std::cout << info_str << ": ";
    for (int i = 0; i < 8; i++) {
      std::cout << res[i] << ' ';
    }
    std::cout << std::endl;
  }

  static void print_ratio_mat(
      const std::string &info_str_1, const std::string &info_str_2,
      const std::vector<Eigen::VectorXi> &used,
      const std::vector<Eigen::VectorXi> &total) {
    std::cout << info_str_1 << ": " << std::endl;
    for (int i = 0; i < used.size(); i++) {
      std::cout << info_str_2 << ' ' << i << ":\t";
      for (int j = 0; j < 8; j++) {
        std::cout << (double)used[i][j] * 100 / total[i][j] << "%\t";
      }
      std::cout << std::endl;
    }
  }

  static void print_ratio_vec(
      const std::string &info_str, const Eigen::VectorXi &used,
      const Eigen::VectorXi &total) {
    std::cout << info_str << ":  \t";
    for (int i = 0; i < 8; i++) {
      std::cout << (double)used[i] * 100 / total[i] << "%\t";
    }
  }

  // 资源相关

  static bool check_all_fpgas_resources(
      const std::vector<Eigen::VectorXi> &fpgas_resources,
      const std::vector<Eigen::VectorXi> &required_res) {
    for (int i = 0; i < fpgas_resources.size(); i++) {
      if (!Utils::check_single_fpga_resource(
              fpgas_resources[i], required_res[i])) {
        return false;
      }
    }
    return true;
  }

  static bool check_all_fpgas_resources(
      const std::vector<Eigen::VectorXi> &fpgas_resources,
      const std::vector<Node> &nodes,
      const std::vector<std::unordered_set<int>> &assignments) {
    for (int i = 0; i < assignments.size(); i++) {
      if (!Utils::check_single_fpga_resource(
              fpgas_resources[i], assignments[i], nodes)) {
        return false;
      }
    }
    return true;
  }

  static bool check_single_fpga_resource(
      const Eigen::VectorXi &resources, const Eigen::VectorXi &required_res) {
    if ((resources - required_res).minCoeff() < 0) {
      return false;
    }
    return true;
  }

  static bool check_single_fpga_resource(
      const Eigen::VectorXi &resources,
      const std::unordered_set<int> &assignment,
      const std::vector<Node> &nodes) {
    Eigen::VectorXi total_res = Eigen::VectorXi::Zero(8);
    for (const auto &x : assignment) {
      total_res += nodes[x].resources;
      if ((total_res - resources).minCoeff() < 0) {
        return false;
      }
    }
    return true;
  }

  static void get_all_fpgas_res_violations(
      const std::vector<Eigen::VectorXi> &fpgas_resources,
      const std::vector<Eigen::VectorXi> &required_res,
      std::vector<int> &violation_fpgas,
      std::vector<std::vector<int>> &violation_fpgas_res) {
    for (int i = 0; i < fpgas_resources.size(); i++) {
      std::vector<int> violation_res;
      get_single_fpga_violations(
          fpgas_resources[i], required_res[i], violation_res);
      if (!violation_res.empty()) {
        violation_fpgas.push_back(i);
        violation_fpgas_res.push_back(violation_res);
      }
    }
  }

  static void get_all_fpgas_res_violations(
      const std::vector<Eigen::VectorXi> &fpgas_resources,
      const std::vector<Node> &nodes,
      const std::vector<std::unordered_set<int>> &assignments,
      std::vector<int> &violation_fpgas,
      std::vector<std::vector<int>> &violation_fpgas_res) {
    for (int i = 0; i < assignments.size(); i++) {
      std::vector<int> violation_res;
      get_single_fpga_violations(
          fpgas_resources[i], assignments[i], nodes, violation_res);
      if (!violation_res.empty()) {
        violation_fpgas.push_back(i);
        violation_fpgas_res.push_back(violation_res);
      }
    }
  }

  static void get_single_fpga_violations(
      const Eigen::VectorXi &resources, const Eigen::VectorXi &required_res,
      std::vector<int> &violation_res) {
    for (int i = 0; i < 8; i++) {
      if (required_res[i] > resources[i]) {
        violation_res.push_back(i);
      }
    }
  }

  static void get_single_fpga_violations(
      const Eigen::VectorXi &resources,
      const std::unordered_set<int> &assignment, const std::vector<Node> &nodes,
      std::vector<int> &violation_res) {
    Eigen::VectorXi total_res = Eigen::VectorXi::Zero(8);
    for (const auto &x : assignment) {
      total_res += nodes[x].resources;
    }
    for (int i = 0; i < 8; i++) {
      if (total_res[i] > resources[i]) {
        violation_res.push_back(i);
      }
    }
  }

  // hop相关

  static bool check_all_hops(
      const std::vector<Net> &nets, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist, const int max_hops) {
    for (auto &net : nets) {
      if (!check_single_hop(net, parts, dist, max_hops)) {
        return false;
      }
    }
    return true;
  }

  static bool check_single_hop(
      const Net &net, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist, const int max_hops) {
    int src_fpga = parts[net.nodes[0]];
    std::unordered_set<int> involved_fpgas;
    int hop_count = 0;
    for (auto &node : net.nodes) {
      if (involved_fpgas.find(parts[node]) == involved_fpgas.end()) {
        involved_fpgas.insert(parts[node]);
        hop_count += dist[src_fpga][parts[node]];
        if (hop_count > max_hops) {
          return false;
        }
      }
    }
    return true;
  }

  static int get_total_hop_length(
      const std::vector<Net> &nets, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist, const int max_hops,
      std::vector<int> &violation_nets) {
    int total_hop_length = 0;
    for (int i = 0; i < nets.size(); i++) {
      int hop_count = get_single_hop_count(nets[i], parts, dist);
      if (hop_count > max_hops) {
        violation_nets.push_back(i);
      }
      total_hop_length += hop_count * nets[i].weight;
    }
    return total_hop_length;
  }

  static int get_single_hop_length(
      const Net &net, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist) {
    return get_single_hop_count(net, parts, dist) * net.weight;
  }

  static int get_single_hop_count(
      const Net &net, const std::vector<int> &parts,
      const std::vector<std::vector<int>> &dist) {
    int src_fpga = parts[net.nodes[0]];
    std::unordered_set<int> involved_fpgas;
    int hop_count = 0;
    for (auto &node : net.nodes) {
      if (involved_fpgas.find(parts[node]) == involved_fpgas.end()) {
        involved_fpgas.insert(parts[node]);
        hop_count += dist[src_fpga][parts[node]];
      }
    }
    return hop_count;
  }
};
