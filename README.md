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

运行时需要将bin/MtKaHypar拷贝到build/bin/目录下 (update: 现在只用lib 不用mt的bin了 它们时间相差不大 一致性挺高的)  
借助mtkahypar有两种方式 一种是使用mt提供的lib接口 另一种是直接使用mt可执行文件  
由于目前mt的lib接口存在一些问题  
即不能保证优化指标为steiner_tree时 划分的确定性  
所以暂时使用mt可执行文件  
同时为避免参数的混乱 mt的部分参数暂写死在源文件中

程序运行生成题目要求的design.fpga.out文件  
除此以外 还会在当前目录下生成mt的划分结果mt_results.txt文件  
如果使用mt的bin 还会在当前目录下生成mt的输入文件mt_input_hypergraph.txt和mt_input_target_graph.txt

使用mt的lib在case04中还会出现内存释放错误的问题 可能是内存超限的原因 真实原因未知

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

8. instances/: 实例文件 mt项目中的图和超图实例数据

9. lib/: 外部库
- 目前只包含libmtkahypar.so

10. scripts/: 脚本文件  
- scripts/evaluate.sh:  
运行评估器evaluator  
需在build/bin/目录下运行  
使用-t参数指定测试数据路径 使用-s参数指定输出的design.fpga.out文件路径  
其功能是评估design.fpga.out结果是否合法  

- scripts/mt_run.sh:
运行可执行文件bin/MtKaHyPar  
需在build/bin/目录下运行  
根据需要修改参数

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

11. src/: 源文件

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

mt有五种划分模式 我们需要关注的是确定性划分determinstic  
该模式可以保证每次划分的结果相同 也就是没有随机数的影响 当然效果可能会差些  

后期为测试程序的正确性可以改成纯随机 即改为quality + random seed  
如果随机(包括自带的随机和seed的随机)仍能保证效果稳定 且质量显著高于determinstic 可以考虑改为随机

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
比如确定性划分时 对于特定的样例可以尝试调整种子找到一个相对更优的解

点权如何赋值也是一个值得考虑的因素  
old思路是将点权定义为所有资源的代数和  
new思路是将点权定义为占用比例最大的资源的比例乘以总资源

imbanlance是个值得关注的参数  
如果为了方便 直接在mt划分后就使资源尽量不超限 可以尝试把它设为很小的一个值 当然即使如此由于是多维资源仍然会超限  
当然 也可以根据资源进行一个粗略的计算  
最初的思路是设置为设置为总资源比平均资源多出的比例  
后来改为最紧张的那个资源比该资源的平均资源多出的比例  
再优化后改为这个比例除以8 以优化资源数的影响  
当然实际上哪种方法都仍然会超限 因为即使是e=0也会超限

如果后期有很好的调整的方法 也可以将它设得很大 给mt更大的优化自由度

微调思路:  
先保证资源限制 对资源微调  
对于超过资源的fpga 选取某些点 将其移动到相邻(或相距k个fpga)的fpga上  
对于不同的选法和移法 需要使得hop尽量小 或者资源尽量充裕  
暂时仅尝试保证前者 因为为使hop数尽量小 资源不平衡度大一点实际上不是我们关心的  
那么最关键的就是选点和移点 实际上我们可以直接遍历所有点 并遍历所有相邻的fpga  
(由于这一步是静态的 那么还要考虑对多个超限的fpga的操作顺序)  

那么 这个问题可以抽象为以下的背包问题:  
有n(<=1e6)个物品: m(<=64)个背包。每个物品放在每个背包中的价值为w[i][j]，每个物品会占用一定的体积c[i]，每个背包有一个最大容量v[i]。现在需要从这些物品中选一些物品放到背包中，同时要求所选取的物品的体积之和必须大于等于k，且每个背包不能超过最大容量。求使得收益值尽可能大的放法。  

复习 背包问题(一种组合优化问题):  
n个物品 每个物品的费用为c[i] 价值为w[i] 背包总容量为v 求可获得的最大价值  
01背包: 每个物品只能取一次  
完全背包: 每个物品可以取无数次
多重背包: 每个物品可以取有限次  
需要使用动态规划解决 复杂度O(nv)  
对于完全背包 如果物品费用依次成倍数关系 还可以用贪心做  
对于其他背包 贪心可以得到一个近似的解  

多维费用背包: 每个物品有多种费用c[i][j] 背包容量也有多维指标v[i] 做法: 增加维度即可 复杂度O(n*v[0]...v[n-1])  
取物品的总个数限制也可以看成是费用的一个维度  

参考背包九讲: https://www.cnblogs.com/coded-ream/p/7208019.html  

上面抽象的问题实际上是多背包问题(MKP问题)  
多维背包问题/多约束背包问题/多背包问题(MKP问题): 有多个背包 每个背包容量为v[i] 每个物品放入每个背包的价值不同 为w[i][j]  
该类背包问题较为复杂 且求解复杂度高(NP-Hard) 通常使用启发式算法  

我们暂只考虑使用贪心算法  
首先对于多个超限的fpga 我们计算这些fpga超限的不同的资源平均下来的比例 然后按降序排序 这就是我们遍历的顺序  
然后依次考虑每个fpga 遍历该fpga上的所有点(约1e6/64个) 计算将这些点移动到相邻fpga(甚至所有fpga)所改变的hop数  
该步涉及这些点所关联的所有net 根据测试 case02和case03的node平均used by net数约为15 case04约为3  
这里甚至还可以考虑直接按降序对所有fpga都执行这样的操作 以企图优化hop数 但是限于net数量巨大 这可能并不可行  

对于hop数 也采用类似的策略  


## 性能测试

2024.11.28  

- deterministic:  

  - 12 12 / 0 0 / 0.071  
  - 18 23 / 1 3 / 0.075  
  - 1152 1197 / 3 9 / 0.260  
  - 9355 12543 / 0 155 / 3.534  
  - 32104 36832 / 0 669 / 49.547  

- default:  

  - 1147 1248 / 3 8 / 0.175  
    *1117 1191 / 3 8 / 0.173*  
    1096 1146 / 3 6 / 0.170  

  - 8967 11868 / 0 144 / 3.069  
    *9327 12095* / 0 239 / 2.673  
    9508 12560 / 0 *191* / *2.857*  

  - 31044 *36317* / 0 585 / 732.116  
    30767 36315 / *0 642 / 736.042*  
    *30990* 36464 / 0 854 / 755.657  

- quality:  

  - 1130 1194 / 3 7 / 0.230  
    1125 1165 / *3 7 / 0.200*  
    *1129 1189* / 3 11 / 0.199  

  - 9680 *12714* / 0 *219* / 4.878  
    *9556* 12925 / 0 235 / *4.397*  
    9186 12382 / 0 170 / 3.956

  - 30968 35995 / 0 538 / *1565.993*  
    30560 36198 / 0 678 / 1481.959  
    *30707 36062 / 0 591* / 1595.924  

*时间是debug版本的 略慢

分析:  
可见default和quality在增加了随机性的同时 性能并没有很大提升 甚至更差 且耗时增加 尤其是样例四  
而对于我们的需求我们不想要随机 且后续操作不确定性会更大 而mt只能优化steiner tree这一个指标

eps是一个很关键的参数 调整它即在资源约束和hop数之间权衡 它将取决于资源约束调整的效果  
目前有四种设置方法 然而实际上都只是一个数值而已  

2024.12.1  

- 仅考虑资源超限的fpga:
  - eps = 1  
    **raw: 1152 1197 / 3 9 // 9355 12543 / 0 155**
    - all at once  
      desc 3152 79  
      asc 3333 103  
      01 3115 69  
      10 3115 69  
      11 3143 79  
    - one by one  
      00 2983 80  
      01 2983 80  
      10 2975 79  
      11 2983 80  
  - eps = 2  
    **raw: 716 755 / 2 4 // 7989 11342 / 1 175**
    - all at once  
      00 3451 83  
      01 3460 83  
      10 3452 83  
      11 3393 77  
    - one by one  
      00 2880 71  
      01 2870 71  
      10 2870 71  
      11 2870 71  
  - eps = 3  
    **raw: 161 197 / 1 5 // 5670 7643 / 12 139**
    - all at once  
      00 6311 321  43731 1981  
      01 6273 316  43336 1931  
      10 6165 309  43600 1990  
      11 6182 316  43437 1936  
    - one by one  
      00 3185 93  ...
      01 3566 132  ...
      10 3379 77  ...
      11 3269 121  ...
  - eps = 4  
    **raw: 72 89 / 1 2 // 5457 7176 / 13 80**
    - all at once 5323 247
    - one by one 3517 124
- 考虑所有fpga: **not update!**
  - eps = 1  
    - all at once  
      desc 4503 185  10880 102  
      asc 5522 271  10884 104  
      01 10880 102  
      10 10882 102  
      11 10882 102  
    - one by one 2895 64  10868 102
  - eps = 2  
    - all at once 4898 264  12104 240
    - one by one 2835 67  10502 138
  - eps = 3  
    - all at once 5476 339  ...
    - one by one 3435 124  ...
  - eps = 4  
    - all at once 5923 342  ...
    - one by one 3356 69  ...
