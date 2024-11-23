#ifndef TYPEDEFS_H
#define TYPEDEFS_H

typedef enum {
  STATIC_GRAPH,
  DYNAMIC_GRAPH,
  STATIC_HYPERGRAPH,
  DYNAMIC_HYPERGRAPH,
  NULLPTR_HYPERGRAPH
} mt_kahypar_hypergraph_type_t; // 超图类型

typedef enum {
  MULTILEVEL_GRAPH_PARTITIONING,
  N_LEVEL_GRAPH_PARTITIONING,
  MULTILEVEL_HYPERGRAPH_PARTITIONING,
  N_LEVEL_HYPERGRAPH_PARTITIONING,
  LARGE_K_PARTITIONING,
  NULLPTR_PARTITION
} mt_kahypar_partition_type_t; // 分区类型

struct mt_kahypar_context_s; // 上下文
typedef struct mt_kahypar_context_s mt_kahypar_context_t;
struct mt_kahypar_target_graph_s; // 目标图
typedef struct mt_kahypar_target_graph_s mt_kahypar_target_graph_t;

struct mt_kahypar_hypergraph_s;
typedef struct {
  mt_kahypar_hypergraph_s *hypergraph;
  mt_kahypar_hypergraph_type_t type;
} mt_kahypar_hypergraph_t; // 超图

typedef struct {
  const mt_kahypar_hypergraph_s *hypergraph;
  mt_kahypar_hypergraph_type_t type;
} mt_kahypar_hypergraph_const_t; // 常量超图

struct mt_kahypar_partitioned_hypergraph_s;
typedef struct {
  mt_kahypar_partitioned_hypergraph_s *partitioned_hg;
  mt_kahypar_partition_type_t type;
} mt_kahypar_partitioned_hypergraph_t; // 分区超图

typedef struct {
  const mt_kahypar_partitioned_hypergraph_s *partitioned_hg;
  mt_kahypar_partition_type_t type;
} mt_kahypar_partitioned_hypergraph_const_t; // 常量分区超图

typedef unsigned long int mt_kahypar_hypernode_id_t; // 节点
typedef unsigned long int mt_kahypar_hyperedge_id_t; // 超边
typedef int mt_kahypar_hypernode_weight_t; // 点权
typedef int mt_kahypar_hyperedge_weight_t; // 边权
typedef int mt_kahypar_partition_id_t; // 分区

/**
 * Configurable parameters of the partitioning context.
 * 分区上下文的可配置参数
 */
typedef enum {
  // number of blocks of the partition
  // 分区块数
  NUM_BLOCKS,
  // imbalance factor
  // 不平衡因子
  EPSILON,
  // objective function (either 'cut' or 'km1')
  // 目标函数
  OBJECTIVE,
  // number of V-cycles
  // V-cycle轮数
  NUM_VCYCLES,
  // disables or enables logging
  // 是否启用日志
  VERBOSE
} mt_kahypar_context_parameter_type_t;

/**
 * Supported objective functions.
 * 目标函数
 */
typedef enum { CUT, KM1, SOED } mt_kahypar_objective_t;

/**
 * Preset types for partitioning context.
 * 分区上下文的预设类型
 */
typedef enum {
  // deterministic partitioning mode (corresponds to Mt-KaHyPar-SDet)
  // 确定性分区
  DETERMINISTIC,
  // partitioning mode for partitioning a (hyper)graph into a large number of
  // blocks
  // 大K分区
  LARGE_K,
  // computes good partitions very fast (corresponds to Mt-KaHyPar-D)
  // 默认
  DEFAULT,
  // extends default preset with flow-based refinement
  // -> computes high-quality partitions (corresponds to Mt-KaHyPar-D-F)
  // 质量
  QUALITY,
  // n-level code with flow-based refinement
  // => highest quality configuration (corresponds to Mt-KaHyPar-Q-F)
  // 最高质量
  HIGHEST_QUALITY
} mt_kahypar_preset_type_t;

/**
 * Supported (hyper)graph file formats.
 * 图文件格式
 */
typedef enum {
  // Standard file format for graphs
  // 普通图
  METIS,
  // Standard file format for hypergraphs
  // 超图
  HMETIS
} mt_kahypar_file_format_type_t;

#ifndef MT_KAHYPAR_API
#if __GNUC__ >= 4
#define MT_KAHYPAR_API __attribute__((visibility("default")))
#else
#define MT_KAHYPAR_API
#endif
#endif

#endif // TYPEDEFS_H