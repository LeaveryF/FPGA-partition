# 运行可执行文件bin/MtKaHyPar  
# 需在build/bin/目录下运行  
# 根据需要修改参数
../../bin/MtKaHyPar \
  -h ../../examples/ibm01.hgr \
  --preset-type=deterministic \
  -t 4 \
  -k 8 \
  -e 0.03 \
  -g ../../examples/target.graph \
  -o steiner_tree \
  # -g ./instances/graph_no_newline.graph \
  # -o km1 \