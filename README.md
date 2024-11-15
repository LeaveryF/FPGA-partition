# BUPT_Part

## 项目构建

加载CMakeList.txt 使用cmake构建  
将在build/bin/下生成可执行二进制文件partitioner

## 项目运行

Usage: ./partitioner -t \<input_dir> -s \<output-file>

```bash
cd build/bin/ # 切换到可执行文件目录下
./partitioner -t ../../data/sample01 -s ./design.fpga.out # 运行可执行文件
```

或使用scripts中的脚本:

```bash
cd build/bin/ # 切换到可执行文件目录下
../../scripts/run.sh # 运行脚本
```

或使用cmake插件自动配置运行:

```jsonc
  // 加到.vscode/settings.json中
  "cmake.debugConfig": {
    "args": [
      "-t",
      "../../data/sample01",
      "-s",
      "./design.fpga.out",
    ]
  },
```

## 项目结构

1. build/: 构建目录 由cmake生成
- build/bin/: 存放生成的二进制文件

2. config/: mtkahypar的配置文件

3. data/: 公开的测试数据

4. etc/: 赛题服务器提供的其他资源

5. examples/: mtkahypar的示例代码及测试数据

6. include/: 头文件
- libmtkahypar.h: mtkahypar库的头文件
- libmtkahypartypes.h: mtkahypar类型的头文件

7. lib/: 外部库
- 目前只包含libmtkahypar.so

8. scripts/: 脚本文件  
- scripts/evaluate.sh:  
运行评估器evaluator  
需在build/bin/目录下运行  
使用-t参数指定测试数据路径 使用-s参数指定输出的design.fpga.out文件路径  
其功能是评估design.fpga.out结果是否合法  

- scripts/run.sh:  
运行项目生成的可执行文件partitioner  
需在build/bin/目录下运行  
使用-t参数指定测试数据路径 使用-s参数指定输出路径  
会在build/bin/目录下生成结果文件design.fpga.out  

- scripts/scp.sh:  
将项目生成的可执行文件partitioner上传到赛题服务器s3.edachallenge.cn  
需在build/bin/目录下运行  
如有提交到服务器的需求可运行此脚本  
也可以上传其他文件至服务器  

-scripts/update.sh:  
更新赛题服务器s3.edachallenge.cn上的evaluator  
需在build/bin/目录下运行  
如需更新evaluator可运行此脚本  
也可以下载服务器上的其他文件  

9. src/: 源文件

## 赛题服务器相关说明

赛队账号: eda240327@s3.edachallenge.cn  
提交账号: eda240327_submission@s3.edachallenge.cn  

可执行文件提交到~/submission/bin/目录下, 即:  
/home/eda240327_submission/submission/bin/partitioner  

其它文件提交到~/submission/目录下, 如:  
/home/eda240327_submission/submission/eda240327.pdf  
/home/eda240327_submission/submission/README  
/home/eda240327_submission/submission/...  

evaluator在:  
/home/public/evaluator/evaluator  

testcase在:  
/home/public/testcase/  

可执行文件依赖于 libmtkahypar.so 外部库  
已上传至赛题服务器的~/submission/bin/目录  
评测时会将~/submission/bin/加到环境变量  
自测时通过export模拟(已经写在服务器上的~/submission/bin/run.sh脚本中)  
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/eda240327_submission/submission/bin
```
服务器上的run.sh和evaluate.sh使用方法同本地  

如需将生成的可执行文件提交到赛题服务器 执行:  
```bash
cd build/bin/ # 切换到可执行文件目录下
../../scripts/scp.sh # 运行脚本
```

赛题服务器的evaluator可能更新 如需获取最新的evaluator 执行:
```bash
cd build/bin/ # 切换到可执行文件目录下
../../scripts/update.sh # 运行脚本
```

scp.sh和update.sh也可以用于上传和下载服务器上的其他文件
