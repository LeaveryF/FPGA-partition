# 运行项目生成的可执行文件partitioner  
# 需在build/bin/目录下运行  
# 使用-t参数指定测试数据路径 使用-s参数指定输出路径  
# 会在build/bin/目录下生成结果文件design.fpga.out  
./partitioner \
  -t ../../data/case01 \
  -s ./design.fpga.out \

# 赛题服务器运行命令
# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/eda240327_submission/submission/bin
# ./partitioner \
#   -t /home/public/testcase/case01 \
#   -s ./design.fpga.out \