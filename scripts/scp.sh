# 将项目生成的可执行文件partitioner上传到赛题服务器s3.edachallenge.cn  
# 需在build/bin/目录下运行  
# 如有提交到服务器的需求可运行此脚本  
# 也可以上传其他文件至服务器  
scp \
  ./partitioner \
  eda240327_submission@s3.edachallenge.cn:/home/eda240327_submission/submission/bin/partitioner \