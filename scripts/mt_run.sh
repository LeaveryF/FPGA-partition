# 运行可执行文件bin/MtKaHyPar  
# 需在build/bin/目录下运行  
../../bin/MtKaHyPar \
  -h ../../examples/ibm01.hgr \
  --preset-type=deterministic \
  -t 4 \
  -k 8 \
  -e 0.03 \
  -g ../../examples/target.graph \
  -o steiner_tree \
  --write-partition-file=true \
  --partition-output-folder=. \
  # -o km1 \