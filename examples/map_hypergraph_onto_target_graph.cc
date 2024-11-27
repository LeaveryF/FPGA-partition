#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "libmtkahypar.h"

int main(int argc, char *argv[]) {
  // Initialize thread pool
  mt_kahypar_initialize_thread_pool(4, true);
  // Setup partitioning context
  mt_kahypar_context_t *context = mt_kahypar_context_new();
  mt_kahypar_load_preset(context, DETERMINISTIC);
  // mt_kahypar_configure_context_from_file(context, "config.ini");
  mt_kahypar_set_partitioning_parameters(context, 8, 0.03, KM1);
  // Enable logging
  mt_kahypar_set_context_parameter(context, VERBOSE, "1");
  // Set seed
  // mt_kahypar_set_seed(0);

  mt_kahypar_hypergraph_t hypergraph;
  bool use_file = true;
  std::string hgr_file = "../../examples/ibm01.hgr";
  if (use_file) {
    hypergraph = mt_kahypar_read_hypergraph_from_file(
        hgr_file.c_str(), DETERMINISTIC, HMETIS);
  } else {
    std::ifstream fin(hgr_file);
    std::string line;

    std::getline(fin, line);
    std::istringstream iss(line);

    int m, n, sum = 0;
    iss >> m >> n;
    std::vector<std::vector<int>> edges;
    while (std::getline(fin, line)) {
      std::istringstream iss(line);
      std::vector<int> edge;
      int v;
      while (iss >> v) {
        edge.push_back(v - 1);
      }
      sum += edge.size();
      edges.push_back(edge);
    }

    std::unique_ptr<size_t[]> hyperedge_indices =
        std::make_unique<size_t[]>(m + 1);
    size_t cnt = 0;
    for (int i = 0; i < m; i++) {
      hyperedge_indices[i] = cnt;
      cnt += edges[i].size();
    }
    hyperedge_indices[m] = cnt;

    std::unique_ptr<mt_kahypar_hyperedge_id_t[]> hyperedges =
        std::make_unique<mt_kahypar_hyperedge_id_t[]>(sum);
    int index = 0;
    for (const auto &v : edges) {
      for (const auto &e : v) {
        hyperedges[index++] = e;
      }
    }

    hypergraph = mt_kahypar_create_hypergraph(
        DETERMINISTIC, n, m, hyperedge_indices.get(), hyperedges.get(), nullptr,
        nullptr);
  }

  // Read target graph file
  mt_kahypar_target_graph_t *target_graph =
      mt_kahypar_read_target_graph_from_file("../../examples/target.graph");

  // Map hypergraph onto target graph
  mt_kahypar_partitioned_hypergraph_t partitioned_hg =
      mt_kahypar_map(hypergraph, target_graph, context);

  // Extract Mapping
  std::unique_ptr<mt_kahypar_partition_id_t[]> mapping =
      std::make_unique<mt_kahypar_partition_id_t[]>(
          mt_kahypar_num_hypernodes(hypergraph));
  mt_kahypar_get_partition(partitioned_hg, mapping.get());

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
  for (size_t i = 0; i < 8; ++i) {
    std::cout << "Weight of Block " << i << "   = " << block_weights[i]
              << std::endl;
  }

  mt_kahypar_free_context(context);
  mt_kahypar_free_hypergraph(hypergraph);
  mt_kahypar_free_partitioned_hypergraph(partitioned_hg);
  mt_kahypar_free_target_graph(target_graph);
}