#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <tuple>
#include <vector>

#include "libmtkahypar.h"
#include "libmtkahypartypes.h"

#include "io.h"
#include "utils.h"

class MtPartitioner {
public:
  void mt_partition(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // mt-kahypar partition
    init(finest, fpgas);
    if (this->use_mt_lib) {
      mt_partition_lib(finest, fpgas, parts);
    } else {
      mt_partition_bin(finest, fpgas, parts);
    }
  }

private:
  bool use_mt_lib = true; // 使用mt的lib还是mt的bin
  bool mt_lib_use_files = false; // only when use_mt_lib is true
  bool mt_lib_output_files = true; // only when mt_lib_use_files is false
  mt_kahypar_preset_type_t mt_preset = DETERMINISTIC; // preset type
  double mt_eps = 0; // imbalance // will be assigned
  bool mt_use_max_eps = false; // use max eps
  bool mt_zoom_eps = true; // zoom eps
  int mt_seed = 0; // seed
  bool mt_log = false; // log
  std::string mt_bin_path = "../../bin/MtKaHyPar"; // mt可执行文件路径
  std::string mt_in_hypergraph_file = "mt_input_hypergraph.txt"; // 超图
  std::string mt_in_target_graph_file = "mt_input_target_graph.txt"; // 目标图
  std::string mt_out_file = "mt_results.txt"; // mt结果文件

  void init(const Graph &finest, const FPGA &fpgas) {
    // 实际上这样取eps也并没什么用 但至少是动态变化的了
    if (!this->mt_use_max_eps) {
      this->mt_eps = std::numeric_limits<double>::max();
    } else {
      this->mt_eps = 0;
    }
    Eigen::VectorXd ave_res = finest.required_res.cast<double>() / fpgas.size;
    Eigen::VectorXd res_eps = Eigen::VectorXd::Zero(8);
    for (int i = 0; i < 8; i++) { // 假定每块fpga的8种资源分别差不多
      res_eps[i] = (fpgas.lower_res[i] - ave_res[i]) / ave_res[i];
      if (res_eps[i] < 0) {
        std::cout << "Warning: res " << i << " is tight." << std::endl;
      }
      if (!this->mt_use_max_eps) {
        this->mt_eps = std::min(
            this->mt_eps, std::max(res_eps[i], 0.0)); // eps设置为最紧张的资源
      } else {
        // 直接设置为最大eps的策略
        this->mt_eps = std::max(
            this->mt_eps, std::max(res_eps[i], 0.0)); // eps设置为最宽松的资源
      }
    }
    if (this->mt_zoom_eps) {
      if (!this->mt_use_max_eps) {
        this->mt_eps /= 8; // 减小eps值 直观上和资源种类数相关
      } else {
        this->mt_eps *= 8; // 增大eps值 直观上和资源种类数相关
      }
    }
    std::cout << "Res eps: ";
    for (int i = 0; i < 8; i++) {
      std::cout << res_eps[i] << ' ';
    }
    std::cout << std::endl;
    std::cout << "eps: " << this->mt_eps << std::endl << std::endl;
  }

  void mt_partition_lib(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    std::cout << "Start mt partitioning with lib..." << std::endl << std::endl;

    // Initialize thread pool
    mt_kahypar_initialize_thread_pool(4, true);
    // Setup partitioning context
    mt_kahypar_context_t *context = mt_kahypar_context_new();
    mt_kahypar_load_preset(context, this->mt_preset);
    mt_kahypar_set_partitioning_parameters(
        context, fpgas.size, this->mt_eps, KM1);
    // Enable logging
    mt_kahypar_set_context_parameter(
        context, VERBOSE, std::to_string(this->mt_log).c_str());
    // Set seed
    mt_kahypar_set_seed(this->mt_seed);

    mt_kahypar_hypergraph_t hypergraph; // 超图
    mt_kahypar_target_graph_t *target_graph; // 目标图
    if (this->mt_lib_use_files) {
      // Construct hypergraph
      IO::write_mt_input_hypergraph_file(this->mt_in_hypergraph_file, finest);
      hypergraph = mt_kahypar_read_hypergraph_from_file(
          this->mt_in_hypergraph_file.c_str(), this->mt_preset, HMETIS);
      // Construct target graph
      IO::write_mt_input_target_graph_file(
          this->mt_in_target_graph_file, fpgas);
      target_graph = mt_kahypar_read_target_graph_from_file(
          this->mt_in_target_graph_file.c_str());
    } else {
      // Construct hypergraph
      if (this->mt_lib_output_files) {
        IO::write_mt_input_hypergraph_file(this->mt_in_hypergraph_file, finest);
      }
      Utils::construct_mt_hypergraph(finest, hypergraph, this->mt_preset);
      // Construct target graph
      if (this->mt_lib_output_files) {
        IO::write_mt_input_target_graph_file(
            this->mt_in_target_graph_file, fpgas);
      }
      Utils::construct_mt_target_graph(fpgas, &target_graph);
    }

    // Map hypergraph onto target graph
    mt_kahypar_partitioned_hypergraph_t partitioned_hg =
        mt_kahypar_map(hypergraph, target_graph, context);

    // Extract Mapping
    Utils::get_mt_partition_results(partitioned_hg, parts);

    // just output
    if (this->mt_out_file != "") {
      mt_kahypar_write_partition_to_file(
          partitioned_hg, this->mt_out_file.c_str());
    }

    // Extract Block Weights
    std::unique_ptr<mt_kahypar_hypernode_weight_t[]> block_weights =
        std::make_unique<mt_kahypar_hypernode_weight_t[]>(fpgas.size);
    mt_kahypar_get_block_weights(partitioned_hg, block_weights.get());

    // Compute Metrics
    const double imbalance = mt_kahypar_imbalance(partitioned_hg, context);
    const double steiner_tree_metric =
        mt_kahypar_steiner_tree(partitioned_hg, target_graph);

    if (this->mt_log) {
      std::cout << std::endl;
    }

    // Output Results
    std::cout << "Partitioning Results:" << std::endl;
    std::cout << "Imbalance           = " << imbalance << std::endl;
    std::cout << "Steiner Tree Metric = " << steiner_tree_metric << std::endl;
    // for (size_t i = 0; i < fpgas.size; i++) {
    //   std::cout << "Weight of Block " << i << "   = " << block_weights[i]
    //             << std::endl;
    // }
    std::cout << std::endl;

    // destroy
    mt_kahypar_free_context(context);
    mt_kahypar_free_hypergraph(hypergraph);
    mt_kahypar_free_partitioned_hypergraph(partitioned_hg);
    mt_kahypar_free_target_graph(target_graph);
  }

  void mt_partition_bin(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    std::cout << "Start mt partitioning with bin..." << std::endl << std::endl;

    // 输出hmetis格式的超图文件供mt使用
    IO::write_mt_input_hypergraph_file(this->mt_in_hypergraph_file, finest);

    // 输出metis格式的目标图文件供mt使用
    IO::write_mt_input_target_graph_file(this->mt_in_target_graph_file, fpgas);

    // 划分
    std::ostringstream oss;
    const std::string mt_preset_str[5] = {
        "deterministic", "large_k", "default", "quality", "highest_quality"};
    // clang-format off
    oss << this->mt_bin_path << " -h " << this->mt_in_hypergraph_file
        << " --preset-type=" << mt_preset_str[this->mt_preset]
        << " -t 4"
        << " -k " << fpgas.size
        << " -e " << std::setprecision(20) << this->mt_eps << std::setprecision(2)
        << " -g " << this->mt_in_target_graph_file
        << " -o steiner_tree"
        << " --write-partition-file=true"
        << " --partition-output-folder=."
        << " -v " << this->mt_log
        << " --seed " << this->mt_seed;
    // clang-format on
    std::cout << "Executing cmd: " << oss.str() << std::endl << std::endl;
    if (system(oss.str().c_str()) != 0) {
      exit(1);
    }

    // 重命名输出文件
    oss.str("");
    oss << "mv `find . -type f -name \"*.KaHyPar\" -printf '%T+ %p\\n' "
           "| sort -r | head -n 1 | awk '{print $2}'` "
        << this->mt_out_file;
    std::cout << "Executing cmd: " << oss.str() << std::endl << std::endl;
    if (system(oss.str().c_str()) != 0) {
      exit(1);
    }

    // 读取划分结果
    IO::read_mt_results(this->mt_out_file, parts);
  }
};

class Trimmer {
private:
  bool trim_res_to_all_fpgas = true; // 微调时考虑所有fpga/仅考虑相邻fpga
  bool trim_res_of_all_fpgas = false; // 是否对所有fpga上的所有点微调
  bool trim_res_by_fpgas_asc = false; // 对fpga升序
  bool trim_res_by_nodes_asc = false; // 对节点升序
  bool trim_res_by_gains_asc = false; // 对增益升序

public:
  void trim_res(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts,
      std::vector<std::unordered_set<int>> &assignments,
      std::vector<Eigen::VectorXi> &required_res) {
    std::cout << "Trimming res..." << std::endl << std::endl;

    // 微调资源
    std::vector<int> violation_fpgas;
    std::vector<std::vector<int>> violation_fpgas_res;
    Utils::get_all_fpgas_res_violations(
        fpgas.resources, required_res, violation_fpgas, violation_fpgas_res);

    // 获取rank 暂按照最大资源利用率排序
    std::vector<std::pair<double, int>> fpgas_rank; // <max_ratio, fpga_num>
    for (int i = 0; i < fpgas.size; i++) {
      double max_ratio = 0;
      for (int j = 0; j < 8; j++) {
        max_ratio = std::max(
            max_ratio, (double)required_res[i][j] / fpgas.resources[i][j]);
      }
      fpgas_rank.push_back({max_ratio, i});
    }
    // 按照最大资源利用率排序
    if (this->trim_res_by_fpgas_asc) {
      std::sort(
          fpgas_rank.begin(), fpgas_rank.end(),
          [](const auto &a, const auto &b) { return a.first < b.first; });
    } else {
      std::sort(
          fpgas_rank.begin(), fpgas_rank.end(),
          [](const auto &a, const auto &b) { return a.first > b.first; });
    }
    std::cout << "FPGA res rank: ";
    for (const auto &[_, i] : fpgas_rank) {
      std::cout << i << ' ';
    }
    std::cout << std::endl << std::endl;

    // 对于所有fpga
    for (const auto &[_, i] : fpgas_rank) {
      bool is_violation =
          std::find(violation_fpgas.begin(), violation_fpgas.end(), i) !=
          violation_fpgas.end();
      // 仅微调有冲突的fpga
      if (!is_violation && !this->trim_res_of_all_fpgas) {
        continue;
      }
      if (is_violation) {
        std::cout << "Trimming res of the violation FPGA " << i << "..."
                  << std::endl;
      } else {
        std::cout << "Trimming res of FPGA " << i << "..." << std::endl;
      }
      // 按节点权值排序
      std::vector<int> node_rank(assignments[i].begin(), assignments[i].end());
      if (this->trim_res_by_nodes_asc) {
        std::sort(
            node_rank.begin(), node_rank.end(),
            [&](const int &a, const int &b) {
              return finest.nodes[a].weight < finest.nodes[b].weight;
            });
      } else {
        std::sort(
            node_rank.begin(), node_rank.end(),
            [&](const int &a, const int &b) {
              return finest.nodes[a].weight > finest.nodes[b].weight;
            });
      }
      std::vector<std::tuple<int, int, int>>
          gains_rank; // <node_num, to_fpga, gain>
      // 对于这个fpga上的所有点
      for (const auto &node : node_rank) {
        // 考虑移到所有fpga上
        for (int j = 0; j < fpgas.size; j++) {
          auto gain_tuple = std::make_tuple(node, j, 0);
          // 对于这个点关联的所有超边 减去增加的gain
          int tmp = parts[node];
          parts[node] = j;
          for (const auto &net : finest.incident_edges[node]) {
            std::get<2>(gain_tuple) -= Utils::get_single_hop_length(
                finest.nets[net], parts, fpgas.dist);
          }
          // 加上之前的gain
          parts[node] = tmp;
          for (const auto &net : finest.incident_edges[node]) {
            std::get<2>(gain_tuple) += Utils::get_single_hop_length(
                finest.nets[net], parts, fpgas.dist);
          }
          gains_rank.push_back(gain_tuple);
        }
      }
      // 按照gain排序 gain本身越大收益越大
      if (this->trim_res_by_gains_asc) {
        std::sort(
            gains_rank.begin(), gains_rank.end(),
            [](const auto &t1, const auto &t2) {
              return std::get<2>(t1) < std::get<2>(t2);
            });
      } else {
        std::sort(
            gains_rank.begin(), gains_rank.end(),
            [](const auto &t1, const auto &t2) {
              return std::get<2>(t1) > std::get<2>(t2);
            });
      }

      int pre_total_nodes = assignments[i].size();
      std::unordered_set<int> visited_nodes;
      for (const auto &[node, j, gain] : gains_rank) {
        // 已访问过的节点
        if (visited_nodes.find(node) != visited_nodes.end()) {
          continue;
        }
        // 不能分配到资源已满的fpga上
        if (!Utils::check_single_fpga_resource(
                fpgas.resources[j],
                required_res[j] + finest.nodes[node].resources)) {
          continue;
        }
        visited_nodes.insert(node);
        required_res[i] -= finest.nodes[node].resources;
        required_res[j] += finest.nodes[node].resources;
        assignments[i].erase(node);
        assignments[j].insert(node);
        parts[node] = j;
        // 提前退出
        if (visited_nodes.size() == pre_total_nodes) {
          break;
        }
        // 检查资源
        if (!this->trim_res_of_all_fpgas &&
            Utils::check_single_fpga_resource(
                fpgas.resources[i], required_res[i])) {
          break;
        }
      }
      // 检查资源
      if (!Utils::check_single_fpga_resource(
              fpgas.resources[i], required_res[i])) {
        std::cerr << "Trim res failed, res can't satisfied." << std::endl;
        exit(1);
      }
      // 一定每个结点都会被访问到
      if (this->trim_res_of_all_fpgas &&
          visited_nodes.size() != pre_total_nodes) {
        std::cerr << "Trim res failed, missing nodes." << std::endl;
        exit(1);
      }
    }
    std::cout << std::endl;
  }

  void
  trim_hop(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    std::cout << "Trimming hop..." << std::endl << std::endl;
  }

private:
};

class LogicalReplicator {
public:
  void
  replicate(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {}

private:
};

class Partition {
public:
  static void
  partition(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // mt-kahypar partition
    MtPartitioner mt_partitioner;
    mt_partitioner.mt_partition(finest, fpgas, parts);

    // 获取 分组 和 资源向量
    std::vector<std::unordered_set<int>> assignments(fpgas.size);
    Utils::get_assignments(parts, assignments);
    std::vector<Eigen::VectorXi> required_res;
    Utils::get_required_res(assignments, finest.nodes, required_res);

    bool res_satisfied = check_res(finest, fpgas, required_res, true);
    bool hop_satisfied = check_hop(finest, fpgas, parts, true);

    // trim res
    Trimmer trimmer;
    if (!res_satisfied) {
      trimmer.trim_res(finest, fpgas, parts, assignments, required_res);
      res_satisfied = check_res(finest, fpgas, required_res, true);
      hop_satisfied = check_hop(finest, fpgas, parts, false);
      if (!res_satisfied) {
        std::cerr << "Trim res failed." << std::endl;
        exit(1);
      }
    }

    // trim hop
    // if (!hop_satisfied) {
    //   trimmer.trim_hop(finest, fpgas, parts);
    //   hop_satisfied = check_hop(finest, fpgas, parts, false);
    // }
    // if (!hop_satisfied) {
    //   std::cerr << "Trim hop failed." << std::endl;
    // }

    // logical replicate
    // LogicalReplicator logical_replicator;
    // logical_replicator.replicate(finest, fpgas, parts);
    // satisfied = check(finest, fpgas, parts);
    // if (!satisfied) {
    //   std::cerr << "Logical replicator gave the wrong answer, Partition
    //   failed."
    //             << std::endl;
    //   exit(1);
    // }

    std::cout << "Finish partition." << std::endl << std::endl;
  }

private:
  static bool check_res(
      const Graph &finest, const FPGA &fpgas,
      const std::vector<Eigen::VectorXi> &required_res, bool print_flag) {
    // 检查资源
    if (print_flag) {
      Utils::print_res_mat("FPGA res", "FPGA", fpgas.resources);
      Utils::print_res_vec("Total", fpgas.total_res);
      std::cout << std::endl;

      Utils::print_res_mat("Required res", "Block", required_res);
      Utils::print_res_vec("Total", finest.required_res);
      std::cout << std::endl;

      Utils::print_ratio_mat("Ratio", "Block", required_res, fpgas.resources);
      Utils::print_ratio_vec("Total", finest.required_res, fpgas.total_res);
      std::cout << std::endl;
    }

    std::vector<int> violation_fpgas;
    std::vector<std::vector<int>> violation_fpgas_res;
    Utils::get_all_fpgas_res_violations(
        fpgas.resources, required_res, violation_fpgas, violation_fpgas_res);

    bool satisfied = false;
    if (violation_fpgas.size() == 0) {
      std::cout << "Resources satisfied!" << std::endl;
      satisfied = true;
    } else {
      std::cout << violation_fpgas.size() << " violation fpgas: " << std::endl;
      for (int i = 0; i < violation_fpgas.size(); i++) {
        std::cout << "FPGA " << violation_fpgas[i] << ": ";
        for (auto j : violation_fpgas_res[i]) {
          std::cout << "res" << j << ' ';
        }
        std::cout << std::endl;
      }
    }
    std::cout << std::endl;

    return satisfied;
  }

  static bool check_hop(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts,
      bool print_flag) {
    // 检查hop
    std::vector<int> violation_nets;
    int total_hop_length = Utils::get_total_hop_length(
        finest.nets, parts, fpgas.dist, fpgas.max_hops, violation_nets);
    std::cout << "Total hop length: " << total_hop_length << std::endl
              << std::endl;

    bool satisfied = false;
    if (violation_nets.size() == 0) {
      std::cout << "Hops satisfied!" << std::endl;
      satisfied = true;
    } else {
      std::cout << violation_nets.size() << " violation nets: " << std::endl;
      for (auto i : violation_nets) {
        std::cout << i << ' ';
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;

    return satisfied;
  }
};
