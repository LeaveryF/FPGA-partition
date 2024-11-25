#include <iostream>
#include <memory>
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
  if (use_file) {
    // Load Hypergraph
    hypergraph = mt_kahypar_read_hypergraph_from_file(
        "./instances/hypergraph_with_node_and_edge_weights.hgr", DETERMINISTIC,
        HMETIS);
  } else {
    // In the following, we construct a hypergraph with 7 nodes and 4 hyperedges
    const mt_kahypar_hypernode_id_t num_nodes = 7;
    const mt_kahypar_hyperedge_id_t num_hyperedges = 4;

    // The hyperedge indices points to the hyperedge vector and defines the
    // the ranges containing the pins of each hyperedge
    std::unique_ptr<size_t[]> hyperedge_indices = std::make_unique<size_t[]>(5);
    hyperedge_indices[0] = 0;
    hyperedge_indices[1] = 2;
    hyperedge_indices[2] = 6;
    hyperedge_indices[3] = 9;
    hyperedge_indices[4] = 12;

    std::unique_ptr<mt_kahypar_hyperedge_id_t[]> hyperedges =
        std::make_unique<mt_kahypar_hyperedge_id_t[]>(12);
    // First hyperedge contains nodes with ID 0 and 2
    hyperedges[0] = 0;
    hyperedges[1] = 2;
    // Second hyperedge contains nodes with ID 0, 1, 3 and 4
    hyperedges[2] = 0;
    hyperedges[3] = 1;
    hyperedges[4] = 3;
    hyperedges[5] = 4;
    // Third hyperedge contains nodes with ID 3, 4 and 6
    hyperedges[6] = 3;
    hyperedges[7] = 4;
    hyperedges[8] = 6;
    // Fourth hyperedge contains nodes with ID 2, 5 and 6
    hyperedges[9] = 2;
    hyperedges[10] = 5;
    hyperedges[11] = 6;

    // Define node weights
    std::unique_ptr<mt_kahypar_hypernode_weight_t[]> node_weights =
        std::make_unique<mt_kahypar_hypernode_weight_t[]>(7);
    node_weights[0] = 5;
    node_weights[1] = 8;
    node_weights[2] = 2;
    node_weights[3] = 3;
    node_weights[4] = 4;
    node_weights[5] = 9;
    node_weights[6] = 8;

    // Define hyperedge weights
    std::unique_ptr<mt_kahypar_hyperedge_weight_t[]> hyperedge_weights =
        std::make_unique<mt_kahypar_hyperedge_weight_t[]>(4);
    hyperedge_weights[0] = 4;
    hyperedge_weights[1] = 2;
    hyperedge_weights[2] = 3;
    hyperedge_weights[3] = 8;

    // Construct hypergraph for DEFAULT preset
    hypergraph = mt_kahypar_create_hypergraph(
        DEFAULT, num_nodes, num_hyperedges, hyperedge_indices.get(),
        hyperedges.get(), hyperedge_weights.get(), node_weights.get());
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