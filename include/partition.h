#pragma once

#include <memory>
#include <vector>

#include "libmtkahypar.h"
#include "libmtkahypartypes.h"
#include "util.h"

class Partitioner {
public:
  void
  partition(const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // mt-kahypar partition
    init(finest, fpgas);
    mt_partition(finest, fpgas, parts);
  }

private:
  bool use_mt_lib = true; // 使用mt的lib还是mt的bin
  double eps = 0; // imbalance, for mt // will be assigned
  std::string mt_out_file = "mt_results.txt"; // mt结果文件

  // only when use_mt_lib is true
  int mt_seed = 0; // seed
  bool mt_log = false; // log

  void init(const Graph &finest, const FPGA &fpgas) {
    // 实际上这样取eps也并不严谨 但至少是动态变化的了
    Eigen::VectorXd ave_res = finest.required_res.cast<double>() / fpgas.size;
    Eigen::VectorXd res_eps = Eigen::VectorXd::Zero(8);
    for (int i = 0; i < 8; i++) { // 假定每块fpga的8种资源分别差不多
      res_eps[i] = (fpgas.lower_res[i] - ave_res[i]) / ave_res[i];
      if (res_eps[i] < 0) {
        std::cout << "Warning: res " << i << " is tight." << std::endl;
      }
      this->eps = std::min(
          this->eps, std::max(res_eps[i], 0.0)); // eps设置为最紧张的资源
    }
    std::cout << "Res eps: ";
    for (int i = 0; i < 8; i++) {
      std::cout << res_eps[i] << ' ';
    }
    std::cout << std::endl;
    std::cout << "eps: " << this->eps << std::endl << std::endl;
  }

  void mt_partition_lib(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    // Initialize thread pool
    mt_kahypar_initialize_thread_pool(4, true);
    // Setup partitioning context
    mt_kahypar_context_t *context = mt_kahypar_context_new();
    mt_kahypar_load_preset(context, DETERMINISTIC);
    mt_kahypar_set_partitioning_parameters(context, fpgas.size, this->eps, KM1);
    // Enable logging
    mt_kahypar_set_context_parameter(
        context, VERBOSE, this->mt_log ? "1" : "0");
    mt_kahypar_set_seed(this->mt_seed);

    mt_kahypar_hypergraph_t hypergraph; // 超图
    mt_kahypar_target_graph_t *target_graph; // 目标图

    { // Construct hypergraph
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
      for (int i = 0; i < finest.nets.size(); i++) {
        for (int j = 0; j < finest.nets[i].nodes.size(); j++) {
          hyperedges[index++] = finest.nets[i].nodes[j];
        }
      }
      // 节点权重数组
      std::unique_ptr<mt_kahypar_hypernode_weight_t[]> node_weights =
          std::make_unique<mt_kahypar_hypernode_weight_t[]>(
              finest.nodes.size());
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
          DEFAULT, finest.nodes.size(), finest.nets.size(),
          hyperedge_indices.get(), hyperedges.get(), hyperedge_weights.get(),
          node_weights.get());
    }

    { // Construct target graph
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
      target_graph = mt_kahypar_create_target_graph(
          fpgas.size, fpgas.num_edges, edges.get(), edge_weights.get());
    }

    // Map hypergraph onto target graph
    mt_kahypar_partitioned_hypergraph_t partitioned_hg =
        mt_kahypar_map(hypergraph, target_graph, context);

    { // Extract Mapping
      std::unique_ptr<mt_kahypar_partition_id_t[]> mapping =
          std::make_unique<mt_kahypar_partition_id_t[]>(
              mt_kahypar_num_hypernodes(hypergraph));
      mt_kahypar_get_partition(partitioned_hg, mapping.get());
      for (int i = 0; i < finest.nodes.size(); i++) {
        parts[i] = mapping[i];
      }
      if (this->mt_out_file != "") {
        mt_kahypar_write_partition_to_file(
            partitioned_hg, this->mt_out_file.c_str());
      }
    }

    // Extract Block Weights
    std::unique_ptr<mt_kahypar_hypernode_weight_t[]> block_weights =
        std::make_unique<mt_kahypar_hypernode_weight_t[]>(8);
    mt_kahypar_get_block_weights(partitioned_hg, block_weights.get());

    // Compute Metrics
    const double imbalance = mt_kahypar_imbalance(partitioned_hg, context);
    const double steiner_tree_metric =
        mt_kahypar_steiner_tree(partitioned_hg, target_graph);

    // Output Results
    std::cout << "Partitioning Results:" << std::endl;
    std::cout << "Imbalance           = " << imbalance << std::endl;
    std::cout << "Steiner Tree Metric = " << steiner_tree_metric << std::endl;
    for (size_t i = 0; i < fpgas.size; ++i) {
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
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {}

  void mt_partition(
      const Graph &finest, const FPGA &fpgas, std::vector<int> &parts) {
    if (this->use_mt_lib) {
      mt_partition_lib(finest, fpgas, parts);
    } else {
      mt_partition_bin(finest, fpgas, parts);
    }
  }
};
