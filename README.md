# SMT_paddle

## 模型简介
Scale-Aware Modulation Meet Transformer (SMT) 是一种新型的视觉Transformer架构，它通过结合卷积网络和视觉Transformer的优势，有效处理多种下游任务。SMT的核心是Scale-Aware Modulation (SAM)，它包含两个主要的创新设计：Multi-Head Mixed Convolution (MHMC) 模块和Scale-Aware Aggregation (SAA) 模块。
MHMC模块通过引入不同核大小的多个卷积操作，能够捕获多尺度特征并扩大感受野。这种设计使得网络能够更好地模拟长距离依赖关系。SAA模块则是一种轻量级但有效的设计，它通过在不同头之间聚合特征来实现信息融合，同时保持了网络的轻量化。
此外，SMT提出了一种名为Evolutionary Hybrid Network (EHN)的新型混合网络架构，它能够模拟网络深度增加时从捕获局部到全局依赖关系的转变，从而在各种视觉任务中实现更优的性能。

## 使用方法
1.下载paddleclas，根据paddleclas依赖配置环境，将SMTMODEL复制到paddleclass根目录下。

2.ppcls\configs\ImageNet中包含yaml配置文件的SMT文件夹放入paddleclas对应位置（ppcls\configs\ImageNet）。


## 下载权重
权重文件链接：https://pan.baidu.com/s/1YJiMzHpQiBybPg68UIqHmA?pwd=1111, 提取码：1111 


## 参考
本项目代码参考自：[https://github.com/STVIR/pysot](https://paperswithcode.com/paper/scale-aware-modulation-meet-transformer)
