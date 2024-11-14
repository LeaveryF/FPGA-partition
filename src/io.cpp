#include <fstream>
#include <iostream>
#include <sstream>

#include "io.h"

namespace IO {

IOSystem::IOSystem(
    const string &input_dir, graph &finest, fpga &fpgas,
    flat_hash_map<string, int> &fpga_map, flat_hash_map<string, int> &node_map,
    flat_hash_map<int, string> &fpga_reverse_map,
    flat_hash_map<int, string> &node_reverse_map) {
  read_design_info(
      input_dir + "/design.info", fpgas, fpga_map, fpga_reverse_map);
  read_design_are(
      input_dir + "/design.are", finest, node_map, node_reverse_map);
  read_design_net(input_dir + "/design.net", finest, node_map);
  read_design_topo(input_dir + "/design.topo", fpgas, fpga_map);
}

void IOSystem::write_design_fpga_out(
    const graph &finest, const string &ouput_file, vector<int> &parts,
    const flat_hash_map<int, string> &fpga_reverse_map,
    const flat_hash_map<int, string> &node_reverse_map) {
  ofstream fout(ouput_file);
  if (!fout) {
    cerr << "Cannot write design.fpga.out file." << endl;
    exit(1);
  }

  vector<vector<int>> fpga_assignments(fpga_reverse_map.size());
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i] >= 0) {
      fpga_assignments[parts[i]].push_back(i);
    }
  }

  for (int i = 0; i < fpga_reverse_map.size(); i++) {
    fout << fpga_reverse_map.at(i) << ":";
    for (auto &x : fpga_assignments[i]) {
      if (finest.nodes[x].is_duplicate) {
        fout << " " << node_reverse_map.at(x) << "*";
      } else {
        fout << " " << node_reverse_map.at(x);
      }
    }
    fout << endl;
  }
  spdlog::info("Finish writing design.fpga.out file.");
}

void IOSystem::read_design_info(
    const string &file_path, fpga &fpgas, flat_hash_map<string, int> &fpga_map,
    flat_hash_map<int, string> &fpga_reverse_map) {
  ifstream file(file_path);
  if (!file) {
    cerr << "Cannot find design.info file." << endl;
    exit(1);
  }

  string line;
  VectorXi tmp = VectorXi::Zero(8);
  fpgas.size = 0; // init
  while (getline(file, line)) {
    stringstream ss(line);
    string name;
    int max_interconnects;
    ss >> name >> max_interconnects;
    fpga_reverse_map[fpgas.size] = name;
    fpga_map[name] = fpgas.size++;              // name -> num  start from 0!
    fpgas.maxInterconnects = max_interconnects; // 最大对外互联数

    VectorXi max_resources = VectorXi::Zero(8);
    for (int i = 0; i < 8; ++i) {
      ss >> max_resources[i]; // 8种资源
    }
    tmp += max_resources;

    fpgas.resources.push_back(max_resources); // add to fpgas
  }
  spdlog::info("Finish reading design.info file, {} fpgas.", fpgas.size);
  cout << "Total res: ";
  for (int i = 0; i < 8; i++)
    cout << tmp[i] << ' ';
  cout << endl;
}

void IOSystem::read_design_are(
    const string &file_path, graph &finest,
    flat_hash_map<string, int> &node_map,
    flat_hash_map<int, string> &node_reverse_map) {
  ifstream file(file_path);
  if (!file) {
    cerr << "Cannot find design.are file." << endl;
    exit(1);
  }

  string line;
  VectorXi tmp = VectorXi::Zero(8);
  while (getline(file, line)) {
    stringstream ss(line);
    string name;
    ss >> name;
    node_reverse_map[finest.nodes.size()] = name;
    node_map[name] = finest.nodes.size(); // name -> num  start from 0!

    node node;
    node.resources.resize(8);
    for (int i = 0; i < 8; ++i) {
      ss >> node.resources[i]; // 8种资源
    }
    tmp += node.resources;

    finest.nodes.push_back(node); // add to finest
  }
  spdlog::info(
      "Finish reading design.are file, {} nodes.", finest.nodes.size());
  cout << "Required res: ";
  for (int i = 0; i < 8; i++)
    cout << tmp[i] << ' ';
  cout << endl;
}

void IOSystem::read_design_net(
    const string &file_path, graph &finest,
    const flat_hash_map<string, int> &node_map) {
  ifstream file(file_path);
  if (!file) {
    cerr << "Cannot find design.net file." << endl;
    exit(1);
  }

  string line;
  int max_k = 0, sum_k = 0, cnt_k = 0, sum_ex_k = 0;
  const int _k = 200;
  string max_loc;
  while (getline(file, line)) {
    stringstream ss(line);
    string src;
    int weight;
    ss >> src >> weight; // weighted

    net net;
    net.weight = weight;
    net.nodes.push_back(node_map.at(src)); // 0 is src

    string dest;
    while (ss >> dest) {
      net.nodes.push_back(node_map.at(dest)); // dests
    }

    // test infomation about _k
    if (net.nodes.size() > max_k) {
      max_loc = src;
      max_k = net.nodes.size();
    }
    sum_ex_k += net.nodes.size();
    if (net.nodes.size() > _k) {
      cnt_k++;
      sum_ex_k -= net.nodes.size();
    }
    sum_k += net.nodes.size();

    finest.nets.push_back(net); // add to finest
  }
  spdlog::info("Finish reading design.net file, {} nets.", finest.nets.size());
  cout << "Max_k: " << max_k << "(" << max_loc << "), "
       << "Cnt_k: " << cnt_k << ", "
       << "Sum_k: " << sum_k << ", "
       << "Ave_k: " << (double)sum_k / finest.nets.size() << ", "
       << "Sum_ex_k: " << sum_ex_k << "." << endl;
}

void IOSystem::read_design_topo(
    const string &file_path, fpga &fpgas,
    const flat_hash_map<string, int> &fpga_map) {
  ifstream file(file_path);
  if (!file) {
    cerr << "Cannot find design.topo file." << endl;
    exit(1);
  }

  string line;
  getline(file, line); // Read max hop
  fpgas.maxHops = stoi(line);
  fpgas.topology.resize(fpgas.size, vector<int>(fpgas.size, 0)); // init
  fpgas.cutweights_assignment.resize(
      fpgas.size, vector<int>(fpgas.size, 0)); // init
  int cnt = 0;
  while (getline(file, line)) {
    cnt++;
    stringstream ss(line);
    string s1, s2;
    ss >> s1 >> s2; // read topo
    int id1 = fpga_map.at(s1), id2 = fpga_map.at(s2);
    fpgas.topology[id1][id2] = 1; // attention for undirected graph
    fpgas.topology[id2][id1] = 1;

    fpgas.cutweights_assignment[id1][id2] = 1; // ? all 1
    fpgas.cutweights_assignment[id2][id1] = 1;
  }

  // compute dist, max_dist
  fpgas.dist.resize(fpgas.size, vector<int>(fpgas.size, 0)); // init
  fpgas.maxDist.resize(fpgas.size, 0);                       // init
  for (int i = 0; i < fpgas.size; i++) {                     // bfs求最短路
    queue<pair<int, int>> que;
    vector<bool> visited(fpgas.size);
    que.push({i, 0});
    visited[i] = true;
    pair<int, int> cur;
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
    fpgas.maxDist[i] = cur.second;
  }

  // compute s_hat
  fpgas.S_hat.resize(fpgas.size);
  for (int i = 0; i < fpgas.size; i++) {
    fpgas.S_hat[i].resize(fpgas.maxDist[i] + 1);
    for (int x = 1; x <= fpgas.maxDist[i]; x++) {
      for (int j = 0; j < fpgas.size; j++) {
        if (fpgas.dist[i][j] <= x) {
          fpgas.S_hat[i][x].insert(j);
        }
      }
    }
  }

  // info
  spdlog::info("Finish reading design.topo file, {} arcs.", cnt);
  cout << "Allowed max hop: " << fpgas.maxHops << endl;
  cout << "Topo: " << endl;
  for (int i = 0; i < fpgas.size; i++) {
    for (int j = 0; j < fpgas.size; j++)
      cout << fpgas.topology[i][j] << ' ';
    cout << '\n';
  }
  cout << "Dist: " << endl;
  for (int i = 0; i < fpgas.size; i++) {
    for (int j = 0; j < fpgas.size; j++)
      cout << fpgas.dist[i][j] << ' ';
    cout << '\n';
  }
  cout << "MaxDist: " << endl;
  for (int i = 0; i < fpgas.size; i++)
    cout << fpgas.maxDist[i] << ' ';
  cout << endl;
}

} // namespace IO
