#pragma once

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include "io.h"
#include "libmtkahypar.h"
#include "libmtkahypartypes.h"
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
  bool use_mt_lib = false; // 使用mt的lib还是mt的bin
  double mt_eps = 0; // imbalance, for mt // will be assigned
  int mt_seed = 0; // seed
  bool mt_log = true; // log
  std::string mt_out_file = "mt_results.txt"; // mt结果文件

  // only when use_mt_lib is false
  std::string mt_bin_path = "./MtKaHyPar"; // mt可执行文件路径
  std::string mt_in_hypergraph_file = "mt_input_hypergraph.txt"; // 超图
  std::string mt_in_target_graph_file = "mt_input_target_graph.txt"; // 目标图

  void init(const Graph &finest, const FPGA &fpgas) {
    // 实际上这样取eps也并不严谨 但至少是动态变化的了
    this->mt_eps = std::numeric_limits<double>::max();
    Eigen::VectorXd ave_res = finest.required_res.cast<double>() / fpgas.size;
    Eigen::VectorXd res_eps = Eigen::VectorXd::Zero(8);
    for (int i = 0; i < 8; i++) { // 假定每块fpga的8种资源分别差不多
      res_eps[i] = (fpgas.lower_res[i] - ave_res[i]) / ave_res[i];
      if (res_eps[i] < 0) {
        std::cout << "Warning: res " << i << " is tight." << std::endl;
      }
      this->mt_eps = std::min(
          this->mt_eps, std::max(res_eps[i], 0.0)); // eps设置为最紧张的资源
    }
    this->mt_eps /= 8; // 减小eps值 直观上和资源种类数相关
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
    mt_kahypar_load_preset(context, DETERMINISTIC);
    mt_kahypar_set_partitioning_parameters(
        context, fpgas.size, this->mt_eps, KM1);
    // Enable logging
    mt_kahypar_set_context_parameter(
        context, VERBOSE, std::to_string(this->mt_log).c_str());
    // Set seed
    mt_kahypar_set_seed(this->mt_seed);

    // Construct hypergraph
    mt_kahypar_hypergraph_t hypergraph; // 超图
    Utils::construct_mt_hypergraph(finest, hypergraph);
    // hypergraph = mt_kahypar_read_hypergraph_from_file(
    //     "./mt_input_hypergraph.txt", DETERMINISTIC, HMETIS);

    // Construct target graph
    mt_kahypar_target_graph_t *target_graph; // 目标图
    Utils::construct_mt_target_graph(fpgas, &target_graph);
    // target_graph =
    //     mt_kahypar_read_target_graph_from_file("./mt_input_target_graph.txt");

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

    // Output Results
    std::cout << std::endl;
    std::cout << "Partitioning Results:" << std::endl;
    std::cout << "Imbalance           = " << imbalance << std::endl;
    std::cout << "Steiner Tree Metric = " << steiner_tree_metric << std::endl;
    for (size_t i = 0; i < fpgas.size; i++) {
      std::cout << "Weight of Block " << i << "   = " << block_weights[i]
                << std::endl;
    }
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
    // clang-format off
    oss << this->mt_bin_path << " -h " << this->mt_in_hypergraph_file
        << " --preset-type=deterministic"
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
public:
  static void
  trim(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    std::cout << "Trimming partition results..." << std::endl << std::endl;

    // 检查约束
    std::cout << "Current results report: " << std::endl;
    check(finest, fpgas, parts);

    // 微调

    // 最终检查
    // std::cout << "Final results report" << std::endl;
    // check(finest, fpgas, parts);
  }

private:
  static void
  check(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // 获取 分组 和 资源向量
    // todo 根据使用何种微调策略 调整代码结构
    std::vector<std::unordered_set<int>> assignments(fpgas.size);
    Utils::get_assignments(parts, assignments);
    std::vector<Eigen::VectorXi> required_resources;
    Utils::get_required_resources(
        assignments, finest.nodes, required_resources);

    // 检查资源
    std::cout << "FPGA res: " << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      std::cout << "FPGA " << i << ": ";
      for (int j = 0; j < 8; j++) {
        std::cout << fpgas.resources[i][j] << ' ';
      }
      std::cout << std::endl;
    }
    std::cout << "Total: ";
    for (int i = 0; i < 8; i++) {
      std::cout << fpgas.total_res[i] << ' ';
    }
    std::cout << std::endl << std::endl;

    std::cout << "Required res: " << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      std::cout << "Block " << i << ": ";
      for (int j = 0; j < 8; j++) {
        std::cout << required_resources[i][j] << ' ';
      }
      std::cout << std::endl;
    }
    std::cout << "Total: ";
    for (int i = 0; i < 8; i++) {
      std::cout << finest.required_res[i] << ' ';
    }
    std::cout << std::endl << std::endl;

    std::cout << "Ratio: " << std::endl;
    for (int i = 0; i < fpgas.size; i++) {
      std::cout << "Block " << i << ":\t";
      for (int j = 0; j < 8; j++) {
        std::cout << (double)required_resources[i][j] * 100 /
                         fpgas.resources[i][j]
                  << "%\t";
      }
      std::cout << std::endl;
    }
    std::cout << "Total:  \t";
    for (int i = 0; i < 8; i++) {
      std::cout << (double)finest.required_res[i] * 100 / fpgas.total_res[i]
                << "%\t";
    }
    std::cout << std::endl << std::endl;

    std::vector<int> violation_fpgas;
    std::vector<std::vector<int>> violation_fpgas_res;
    Utils::get_all_fpgas_res_violations(
        fpgas.resources, required_resources, violation_fpgas,
        violation_fpgas_res);

    if (violation_fpgas.size() == 0) {
      std::cout << "Resources satisfied!" << std::endl;
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

    // 检查hop
    std::vector<int> violation_nets;
    int total_hop_length = Utils::get_total_hop_length(
        finest.nets, parts, fpgas.dist, fpgas.max_hops, violation_nets);
    std::cout << "Total hop length: " << total_hop_length << std::endl
              << std::endl;

    if (violation_nets.size() == 0) {
      std::cout << "Hops satisfied!" << std::endl;
    } else {
      std::cout << violation_nets.size() << " violation nets: " << std::endl;
      for (auto i : violation_nets) {
        std::cout << i << ' ';
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }
};

class Partition {
public:
  static void
  partition(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // mt-kahypar partition
    MtPartitioner mt_partitioner;
    mt_partitioner.mt_partition(finest, fpgas, parts);

    // trim
    Trimmer trimmer;
    trimmer.trim(finest, fpgas, parts);

    std::cout << "Finish partition." << std::endl << std::endl;
  }
};
