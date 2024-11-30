#pragma once

#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Sparse>

#include "utils.h"

class IO {
public:
  static void read_input_dir(
      const std::string &input_dir, Graph &finest, FPGA &fpgas,
      std::unordered_map<std::string, int> &fpga_map,
      std::unordered_map<std::string, int> &node_map,
      std::unordered_map<int, std::string> &fpga_reverse_map,
      std::unordered_map<int, std::string> &node_reverse_map) {
    read_design_info(
        input_dir + "/design.info", fpgas, fpga_map, fpga_reverse_map);
    read_design_are(
        input_dir + "/design.are", finest, node_map, node_reverse_map,
        fpgas.total_res);
    read_design_net(input_dir + "/design.net", finest, node_map);
    read_design_topo(input_dir + "/design.topo", fpgas, fpga_map);

    std::cout << "Finish reading input files." << std::endl << std::endl;
  }

  static void write_design_fpga_out(
      const std::string &output_file, const std::vector<int> &parts,
      const std::unordered_map<int, std::string> &fpga_reverse_map,
      const std::unordered_map<int, std::string> &node_reverse_map) {
    std::ofstream fout(output_file);
    if (!fout) {
      std::cerr << "Cannot write file " << output_file << std::endl;
      exit(1);
    }

    std::vector<std::unordered_set<int>> assignments(fpga_reverse_map.size());
    Utils::get_assignments(parts, assignments);

    for (int i = 0; i < fpga_reverse_map.size(); i++) {
      fout << fpga_reverse_map.at(i) << ":";
      for (const auto &x : assignments[i]) {
        fout << ' ' << node_reverse_map.at(x);
      }
      fout << std::endl;
    }
    std::cout << "Finish writing file " << output_file << std::endl
              << std::endl;
  }

  // used by MtPartitioner mt_partition_bin

  static void write_mt_input_hypergraph_file(
      const std::string &file_path, const Graph &finest) {
    std::ofstream fout(file_path);
    if (!fout) {
      std::cerr << "Cannot write file " << file_path << std::endl;
      exit(1);
    }
    fout << finest.nets.size() << ' ' << finest.nodes.size() << ' ' << 11
         << std::endl;
    for (const auto &net : finest.nets) {
      fout << net.weight << ' ';
      for (const auto &node : net.nodes) {
        fout << node + 1 << ' ';
      }
      fout << std::endl;
    }
    for (const auto &node : finest.nodes) {
      fout << node.weight << std::endl;
    }
  }

  static void write_mt_input_target_graph_file(
      const std::string &file_path, const FPGA &fpgas) {
    std::ofstream fout(file_path);
    if (!fout) {
      std::cerr << "Cannot write file " << file_path << std::endl;
      exit(1);
    }
    // metis格式要求第二个参数为无向边的数量 但是实际输入的边数似乎不做限制？
    // 方便起见我们所有双向边都输出一次
    fout << fpgas.size << ' ' << fpgas.num_edges / 2 << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++) {
        if (fpgas.topology[i][j] == 1) {
          fout << j + 1 << ' ';
        }
      }
      fout << std::endl;
    }
  }

  static void
  read_mt_results(const std::string &file_path, std::vector<int> &parts) {
    std::ifstream fin(file_path);
    if (!fin) {
      std::cerr << "Cannot find file " << file_path << std::endl;
      exit(1);
    }
    for (int i = 0; i < parts.size(); i++) {
      fin >> parts[i];
    }
  }

private:
  static void read_design_info(
      const std::string &file_path, FPGA &fpgas,
      std::unordered_map<std::string, int> &fpga_map,
      std::unordered_map<int, std::string> &fpga_reverse_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find file " << file_path << std::endl;
      exit(1);
    }

    std::string line;
    fpgas.total_res.resize(8);
    fpgas.total_res = Eigen::VectorXi::Zero(8);
    fpgas.lower_res.resize(8);
    fpgas.lower_res =
        Eigen::VectorXi::Constant(8, std::numeric_limits<int>::max());
    fpgas.upper_res.resize(8);
    fpgas.upper_res = Eigen::VectorXi::Zero(8);
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string name;
      int max_interconnects;
      ss >> name >> max_interconnects;
      fpga_reverse_map[fpgas.size] = name;
      fpga_map[name] = fpgas.size++; // name -> num  start from 0!
      fpgas.max_interconnects = max_interconnects; // 最大对外互联数

      Eigen::VectorXi res = Eigen::VectorXi::Zero(8);
      for (int i = 0; i < 8; i++) {
        ss >> res[i]; // 8种资源
      }
      fpgas.total_res += res;
      for (int i = 0; i < 8; i++) { // maybe it can use func in eigen
        fpgas.lower_res[i] = std::min(fpgas.lower_res[i], res[i]);
        fpgas.upper_res[i] = std::max(fpgas.upper_res[i], res[i]);
      }

      fpgas.resources.push_back(res); // add to fpgas
    }
    std::cout << "Finish reading file " << file_path << ", " << fpgas.size
              << " fpgas." << std::endl;
    // Utils::print_res_mat("FPGA res", "FPGA", fpgas.resources);
    Utils::print_res_vec("Total res", fpgas.total_res);
    Utils::print_res_vec("Lower res", fpgas.lower_res);
    Utils::print_res_vec("Upper res", fpgas.upper_res);
    std::cout << std::endl;
  }

  static void read_design_are(
      const std::string &file_path, Graph &finest,
      std::unordered_map<std::string, int> &node_map,
      std::unordered_map<int, std::string> &node_reverse_map,
      const Eigen::VectorXi &fpga_total_res) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find file " << file_path << std::endl;
      exit(1);
    }

    std::string line;
    finest.required_res.resize(8);
    finest.required_res = Eigen::VectorXi::Zero(8);
    int total_res = 0;
    for (int i = 0; i < 8; i++) {
      total_res += fpga_total_res[i];
    }
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string name;
      ss >> name;
      node_reverse_map[finest.nodes.size()] = name;
      node_map[name] = finest.nodes.size(); // name -> num  start from 0!

      Node node;
      node.resources.resize(8);
      double max_ratio = 0;
      double min_ratio = std::numeric_limits<double>::max();
      for (int i = 0; i < 8; i++) {
        ss >> node.resources[i]; // 8种资源
        // old: 点权定义为所有资源的代数和
        // node.weight += node.resources[i];
        // new: 点权定义为占用比例最大的资源的比例乘以总资源
        // 因此相比直接使用资源代数和 权值偏大
        if (fpga_total_res[i] != 0) {
          max_ratio = std::max(
              max_ratio, (double)node.resources[i] / fpga_total_res[i]);
          // lower_weight 仅用于计算eps
          min_ratio = std::min(
              min_ratio, (double)node.resources[i] / fpga_total_res[i]);
        }
      }
      node.weight = max_ratio * total_res;
      node.lower_weight = min_ratio * total_res;
      finest.node_weight_sum += node.weight;
      finest.required_res += node.resources;

      finest.nodes.push_back(node); // add to finest
    }
    std::cout << "Finish reading file " << file_path << ", "
              << finest.nodes.size() << " nodes." << std::endl;
    std::cout << "Node weight sum: " << finest.node_weight_sum << std::endl;
    Utils::print_res_vec("Required res", finest.required_res);
    Utils::print_ratio_vec("Ratio", finest.required_res, fpga_total_res);
    std::cout << std::endl;
  }

  static void read_design_net(
      const std::string &file_path, Graph &finest,
      const std::unordered_map<std::string, int> &node_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find file " << file_path << std::endl;
      exit(1);
    }

    std::string line;
    int max_pins = 0, max_used = 0;
    std::string max_node_name, max_used_name;
    finest.incident_edges.resize(finest.nodes.size());
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string src;
      int weight;
      ss >> src >> weight; // weighted

      Net net;
      net.weight = weight;
      net.nodes.push_back(node_map.at(src)); // 0 is src
      // 关联边
      finest.incident_edges[node_map.at(src)].push_back(finest.nets.size());

      std::string dest;
      while (ss >> dest) {
        net.nodes.push_back(node_map.at(dest)); // dests
        // 关联边
        finest.incident_edges[node_map.at(dest)].push_back(finest.nets.size());
        // test info about used
        if (finest.incident_edges[node_map.at(dest)].size() > max_used) {
          max_used_name = dest;
          max_used = finest.incident_edges[node_map.at(dest)].size();
        }
      }

      // test info about pins
      if (net.nodes.size() > max_pins) {
        max_node_name = src;
        max_pins = net.nodes.size();
      }
      finest.pin_size += net.nodes.size();

      finest.nets.push_back(net); // add to finest
    }
    std::cout << "Finish reading file " << file_path << ", "
              << finest.nets.size() << " nets, " << finest.pin_size << " pins."
              << std::endl;
    std::cout << "Max pins: " << max_pins << "(" << max_node_name << "), "
              << "Ave pins: " << (double)finest.pin_size / finest.nets.size()
              << std::endl;
    std::cout << "Max used: " << max_used << "(" << max_used_name << "), "
              << "Ave used: " << (double)finest.pin_size / finest.nodes.size()
              << std::endl
              << std::endl;
  }

  static void read_design_topo(
      const std::string &file_path, FPGA &fpgas,
      const std::unordered_map<std::string, int> &fpga_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find file " << file_path << std::endl;
      exit(1);
    }

    std::string line;
    std::getline(file, line); // Read max hop
    fpgas.max_hops = std::stoi(line);
    fpgas.topology.resize(fpgas.size, std::vector<int>(fpgas.size)); // init
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string s1, s2;
      ss >> s1 >> s2; // read topo

      int id1 = fpga_map.at(s1), id2 = fpga_map.at(s2);
      fpgas.topology[id1][id2] = 1; // attention for undirected graph
      fpgas.topology[id2][id1] = 1; // 输入不保证反向边会存在！
    }

    // compute num_edges and edges
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++) {
        if (fpgas.topology[i][j] == 1) {
          fpgas.num_edges++;
          fpgas.edges.push_back(i);
          fpgas.edges.push_back(j);
        }
      }
    }

    // compute dist
    fpgas.dist.resize(fpgas.size, std::vector<int>(fpgas.size));
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++) {
        if (i == j) {
          fpgas.dist[i][j] = 0;
        } else if (fpgas.topology[i][j] == 1) {
          fpgas.dist[i][j] = 1;
        } else {
          fpgas.dist[i][j] = std::numeric_limits<int>::max() / 2;
        }
      }
    }
    for (int k = 0; k < fpgas.size; k++) {
      for (int i = 0; i < fpgas.size; i++) {
        for (int j = 0; j < fpgas.size; j++) {
          fpgas.dist[i][j] = std::min(
              fpgas.dist[i][j], fpgas.dist[i][k] + fpgas.dist[k][j]); // floyd
        }
      }
    }

    // compute max_dist
    fpgas.max_dist.resize(fpgas.size, 0);
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++) {
        fpgas.max_dist[i] = std::max(fpgas.max_dist[i], fpgas.dist[i][j]);
      }
    }

    // compute s_hat
    fpgas.s_hat.resize(fpgas.size);
    for (int i = 0; i < fpgas.size; i++) {
      fpgas.s_hat[i].resize(fpgas.max_dist[i] + 1);
      for (int x = 0; x <= fpgas.max_dist[i]; x++) {
        for (int j = 0; j < fpgas.size; j++) {
          if (fpgas.dist[i][j] <= x) {
            fpgas.s_hat[i][x].insert(j);
          }
        }
      }
    }

    // info
    std::cout << "Finish reading file " << file_path << ", " << fpgas.num_edges
              << " (as) directed arcs." << std::endl;
    std::cout << "Allowed max hop: " << fpgas.max_hops << std::endl;
    // std::cout << "Topo: " << std::endl;
    // for (int i = 0; i < fpgas.size; i++) {
    //   for (int j = 0; j < fpgas.size; j++) {
    //     std::cout << fpgas.topology[i][j] << ' ';
    //   }
    //   std::cout << std::endl;
    // }
    // std::cout << "Dist: " << std::endl;
    // for (int i = 0; i < fpgas.size; i++) {
    //   for (int j = 0; j < fpgas.size; j++) {
    //     std::cout << fpgas.dist[i][j] << ' ';
    //   }
    //   std::cout << std::endl;
    // }
    // std::cout << "MaxDist: " << std::endl;
    // for (int i = 0; i < fpgas.size; i++) {
    //   std::cout << fpgas.max_dist[i] << ' ';
    // }
    // std::cout << std::endl;
    std::cout << std::endl;
  }
};
