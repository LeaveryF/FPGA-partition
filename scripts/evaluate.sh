# 运行评估器evaluator  
# 需在build/bin/目录下运行  
# 使用-t参数指定测试数据路径 使用-s参数指定输出的design.fpga.out文件路径  
# 其功能是评估design.fpga.out结果是否合法  
../../etc/evaluator \
  -t ../../data/case03 \
  -s ./design.fpga.out \

# 赛题服务器评估命令
# /home/public/evaluator/evaluator \
#   -t /home/public/testcase/case01 \
#   -s ./design.fpga.out \