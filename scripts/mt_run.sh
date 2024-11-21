# 运行可执行文件bin/MtKaHyPar  
# 需在build/bin/目录下运行  
# 根据需要修改参数
../../bin/MtKaHyPar \
  -h ./instances/test_instance.hgr \
  --preset-type=deterministic \
  -t 4 \
  -k 8 \
  -e 0.03 \
  -g ./mt_input_target_graph_3.txt \
  -o steiner_tree \

  # -h ../../examples/ibm01.hgr \
  # -h ./instances/contracted_ibm01.hgr \
  # -h ./instances/delaunay_n15.graph.hgr \
  # -h ./instances/karate_club.graph.hgr \
  # -h ./instances/powersim.mtx.hgr \
  # -h ./instances/sat14_atco_enc1_opt2_10_16.cnf.primal.hgr \
  # -h ./instances/test_instance.hgr \
  # -h ./mt_input_hypergraph.txt \

  # -g ../../examples/target.graph \
  # -g ./instances/graph_no_newline.graph \
  # -g ./mt_input_target_graph.txt \

  # -o km1 \