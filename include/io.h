#pragma once

#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Sparse>

#include "util.h"

class IOSystem {
public:
  void read_input_dir(
      const std::string &input_dir, Graph &finest, FPGA &fpgas,
      std::unordered_map<std::string, int> &fpga_map,
      std::unordered_map<std::string, int> &node_map,
      std::unordered_map<int, std::string> &fpga_reverse_map,
      std::unordered_map<int, std::string> &node_reverse_map) {
    read_design_info(
        input_dir + "/design.info", fpgas, fpga_map, fpga_reverse_map);
    read_design_are(
        input_dir + "/design.are", finest, node_map, node_reverse_map);
    read_design_net(input_dir + "/design.net", finest, node_map);
    read_design_topo(input_dir + "/design.topo", fpgas, fpga_map);
    std::cout << "Finish reading input files." << std::endl << std::endl;
  }

  void write_design_fpga_out(
      const std::string &ouput_file, const std::vector<int> &parts,
      const std::unordered_map<int, std::string> &fpga_reverse_map,
      const std::unordered_map<int, std::string> &node_reverse_map) {
    std::ofstream fout(ouput_file);
    if (!fout) {
      std::cerr << "Cannot write design.fpga.out file." << std::endl;
      exit(1);
    }

    std::vector<std::vector<int>> fpga_assignments(fpga_reverse_map.size());
    for (int i = 0; i < parts.size(); i++) {
      if (parts[i] >= 0) {
        fpga_assignments[parts[i]].push_back(i);
      } else {
        std::cerr << "Node " << i << "'s part invaild. Partition failed."
                  << std::endl;
        exit(1);
      }
    }

    for (int i = 0; i < fpga_reverse_map.size(); i++) {
      fout << fpga_reverse_map.at(i) << ":";
      for (const auto &x : fpga_assignments[i]) {
        fout << " " << node_reverse_map.at(x);
      }
      fout << std::endl;
    }
    std::cout << "Finish writing design.fpga.out file." << std::endl
              << std::endl;
  }

private:
  void read_design_info(
      const std::string &file_path, FPGA &fpgas,
      std::unordered_map<std::string, int> &fpga_map,
      std::unordered_map<int, std::string> &fpga_reverse_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find design.info file." << std::endl;
      exit(1);
    }

    std::string line;
    Eigen::VectorXi tmp = Eigen::VectorXi::Zero(8);
    fpgas.size = 0; // init
    while (getline(file, line)) {
      std::stringstream ss(line);
      std::string name;
      int max_interconnects;
      ss >> name >> max_interconnects;
      fpga_reverse_map[fpgas.size] = name;
      fpga_map[name] = fpgas.size++; // name -> num  start from 0!
      fpgas.max_interconnects = max_interconnects; // 最大对外互联数

      Eigen::VectorXi max_resources = Eigen::VectorXi::Zero(8);
      for (int i = 0; i < 8; ++i) {
        ss >> max_resources[i]; // 8种资源
      }
      tmp += max_resources;

      fpgas.resources.push_back(max_resources); // add to fpgas
    }
    std::cout << "Finish reading design.info file, " << fpgas.size << " fpgas."
              << std::endl;
    std::cout << "Total res: ";
    for (int i = 0; i < 8; i++)
      std::cout << tmp[i] << ' ';
    std::cout << std::endl << std::endl;
  }

  void read_design_are(
      const std::string &file_path, Graph &finest,
      std::unordered_map<std::string, int> &node_map,
      std::unordered_map<int, std::string> &node_reverse_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find design.are file." << std::endl;
      exit(1);
    }

    std::string line;
    Eigen::VectorXi tmp = Eigen::VectorXi::Zero(8);
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string name;
      ss >> name;
      node_reverse_map[finest.nodes.size()] = name;
      node_map[name] = finest.nodes.size(); // name -> num  start from 0!

      Node node;
      node.resources.resize(8);
      for (int i = 0; i < 8; ++i) {
        ss >> node.resources[i]; // 8种资源
      }
      tmp += node.resources;

      finest.nodes.push_back(node); // add to finest
    }
    std::cout << "Finish reading design.are file, " << finest.nodes.size()
              << " nodes." << std::endl;
    std::cout << "Required res: ";
    for (int i = 0; i < 8; i++)
      std::cout << tmp[i] << ' ';
    std::cout << std::endl << std::endl;
  }

  void read_design_net(
      const std::string &file_path, Graph &finest,
      const std::unordered_map<std::string, int> &node_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find design.net file." << std::endl;
      exit(1);
    }

    std::string line;
    int max_k = 0, sum_k = 0, max_u = 0;
    std::string max_node, max_used;
    finest.incident_edges.resize(finest.nodes.size());
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      std::string src;
      int weight;
      ss >> src >> weight; // weighted

      Net net;
      net.weight = weight;
      net.nodes.push_back(node_map.at(src)); // 0 is src

      std::string dest;
      while (ss >> dest) {
        net.nodes.push_back(node_map.at(dest)); // dests
        // 关联边
        finest.incident_edges[node_map.at(dest)].push_back(finest.nets.size());
        // test info about used
        if (finest.incident_edges[node_map.at(dest)].size() > max_u) {
          max_used = dest;
          max_u = finest.incident_edges[node_map.at(dest)].size();
        }
      }

      // test info about pins
      if (net.nodes.size() > max_k) {
        max_node = src;
        max_k = net.nodes.size();
      }
      sum_k += net.nodes.size();

      finest.nets.push_back(net); // add to finest
    }
    std::cout << "Finish reading design.net file, " << finest.nets.size()
              << " nets, " << sum_k << " pins." << std::endl;
    std::cout << "Max pins: " << max_k << "(" << max_node << "), "
              << "Ave pins: " << (double)sum_k / finest.nets.size() << std::endl
              << "Max used: " << max_u << "(" << max_used << ")" << std::endl
              << std::endl;
  }

  void read_design_topo(
      const std::string &file_path, FPGA &fpgas,
      const std::unordered_map<std::string, int> &fpga_map) {
    std::ifstream file(file_path);
    if (!file) {
      std::cerr << "Cannot find design.topo file." << std::endl;
      exit(1);
    }

    std::string line;
    std::getline(file, line); // Read max hop
    fpgas.max_hops = std::stoi(line);
    fpgas.topology.resize(fpgas.size, std::vector<int>(fpgas.size)); // init
    int cnt = 0;
    while (getline(file, line)) {
      cnt++;
      std::stringstream ss(line);
      std::string s1, s2;
      ss >> s1 >> s2; // read topo

      int id1 = fpga_map.at(s1), id2 = fpga_map.at(s2);
      fpgas.topology[id1][id2] = 1; // attention for undirected graph
      fpgas.topology[id2][id1] = 1;
    }

    // compute dist, max_dist
    fpgas.dist.resize(fpgas.size, std::vector<int>(fpgas.size)); // init
    fpgas.max_dist.resize(fpgas.size, 0); // init
    for (int i = 0; i < fpgas.size; i++) { // bfs求最短路
      std::queue<std::pair<int, int>> que;
      std::vector<bool> visited(fpgas.size);
      que.push({i, 0});
      visited[i] = true;
      std::pair<int, int> cur;
      while (!que.empty()) {
        cur = que.front();
        que.pop();
        for (int j = 0; j < fpgas.size; j++) {
          if (fpgas.topology[cur.first][j] == 1 && !visited[j]) {
            que.push({j, cur.second + 1});
            fpgas.dist[i][j] = cur.second + 1;
            visited[j] = true;
          }
        }
      }
      fpgas.max_dist[i] = cur.second;
    }

    // compute s_hat
    fpgas.s_hat.resize(fpgas.size);
    for (int i = 0; i < fpgas.size; i++) {
      fpgas.s_hat[i].resize(fpgas.max_dist[i] + 1);
      for (int x = 1; x <= fpgas.max_dist[i]; x++) {
        for (int j = 0; j < fpgas.size; j++) {
          if (fpgas.dist[i][j] <= x) {
            fpgas.s_hat[i][x].insert(j);
          }
        }
      }
    }

    // info
    std::cout << "Finish reading design.topo file, " << cnt << " arcs."
              << std::endl;
    std::cout << "Allowed max hop: " << fpgas.max_hops << std::endl;
    std::cout << "Topo: " << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++)
        std::cout << fpgas.topology[i][j] << ' ';
      std::cout << '\n';
    }
    std::cout << "Dist: " << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      for (int j = 0; j < fpgas.size; j++)
        std::cout << fpgas.dist[i][j] << ' ';
      std::cout << '\n';
    }
    std::cout << "MaxDist: " << std::endl;
    for (int i = 0; i < fpgas.size; i++)
      std::cout << fpgas.max_dist[i] << ' ';
    std::cout << std::endl;
  }
};
