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

运行时可能需要将bin/MtKaHypar拷贝到build/bin/目录下  
借助mtkahypar有两种方式 一种是使用mt提供的lib接口 另一种是直接使用mt可执行文件  
由于目前mt的lib接口存在一些问题  
即不能保证优化指标为steiner_tree时 划分的确定性  
所以暂时使用mt可执行文件  
同时为避免参数的混乱 mt的参数暂写死在源文件中

## 项目结构

1. bin/: 二进制文件
- 是项目需要用到的二进制文件 目前只包含MtKaHyPar

2. build/: 构建目录 由cmake生成
- build/bin/: 存放生成的二进制文件

3. config/: mtkahypar的配置文件

4. data/: 公开的测试数据

5. etc/: 赛题服务器提供的其他资源

6. examples/: mtkahypar的示例代码及测试数据

7. include/: 头文件
- libmtkahypar.h: mtkahypar库的头文件
- libmtkahypartypes.h: mtkahypar类型的头文件

8. lib/: 外部库
- 目前只包含libmtkahypar.so

9. scripts/: 脚本文件  
- scripts/evaluate.sh:  
运行评估器evaluator  
需在build/bin/目录下运行  
使用-t参数指定测试数据路径 使用-s参数指定输出的design.fpga.out文件路径  
其功能是评估design.fpga.out结果是否合法  

- scripts/mt_run.sh:
运行可执行文件bin/MtKaHyPar  
需在build/bin/目录下运行  

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

10. src/: 源文件

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

可执行文件依赖于 libmtkahypar.so 外部库 以及 MtKaHyPar 二进制文件  
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

## 算法介绍

尝试使用mt  

mt有五种划分模式 我们需要关注的是确定性划分DETERMINISTIC  
该模式可以保证每次划分的结果相同 也就是没有随机数的影响 当然效果可能会差些  

mt有四种优化目标 也就是四种目标函数  
我们重点关注斯坦纳树steiner_tree优化指标  
该指标将原始的超图映射到一个普通图上 然后计算每条超边的各个节点生成的最优斯坦纳树的权值之和  
实际上与我们需要的hop数已经非常接近 我们的要求是从驱动结点到各个结点的hop数 或者说最短路径  
当超边只有两个结点时 二者是相同的 都是最短路 当多个结点时 steiner_tree没有考虑驱动结点

如果可以很好地读mt的代码的话 如果能够直接在mt的代码上修改 效果应该会更好

~~由于在不接触mt的代码的情况下 我们没法使用mt的数据结构 要使用mt的输出 必须从mt输出的文本文件中读取~~  
~~同理 要使用mt可执行文件处理输入 也需要将输入转换为hmetis的文本文件 (尽管mt的lib库有另一种读取方法)~~  
mt的lib可以实现内置的输入输出 mt的bin必须从文本文件输入输出

mt有个ini的配置文件 以及mt的bin有很多命令行参数 后期可以尝试调参

imbanlance是个值得关注的参数 如果为了方便 直接在mt划分后就使资源尽量不超限 可以尝试把它设为很小的一个值  
当然 也可以根据资源进行一个粗略的计算  
如果后期有很好的调整的方法 也可以将它设得很大
