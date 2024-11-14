# 更新赛题服务器s3.edachallenge.cn上的evaluator  
# 需在build/bin/目录下运行  
# 如需更新evaluator可运行此脚本  
# 也可以下载服务器上的其他文件  
scp \
  eda240327_submission@s3.edachallenge.cn:/home/public/evaluator/evaluator \
  ../../etc/evaluator \