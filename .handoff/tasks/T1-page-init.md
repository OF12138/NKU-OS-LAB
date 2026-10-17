# T1 功能模块一 任务一：物理内存探测与页描述符初始化（page_init）
- 负责人：openfar    DDL：10-18    状态：进行中    依赖：T0
- 主要文件：code/kern/mm/pmm.c, report/sections/T1-*

## 要求
指导书原文要点：实现 `static void page_init(void); (kern/mm/pmm.c)`。
- 根据设备树提供的物理内存信息建立 Page 描述符布局，把排除 OpenSBI、内核镜像和描述符数组之后的完整空闲页交给当前 pmm_manager。
- 成功返回时正确设置 `va_pa_offset`、`npage`、`pages`，输出探测到的物理内存范围，完成初始空闲区注册。
- 区分虚拟地址、物理地址、物理页号和 Page 描述符地址；正确处理可管理上界和页面对齐；设备树信息无效时终止初始化；没有完整空闲页时不得调用 `init_memmap()`。
- 可用：`get_memory_base()`、`get_memory_size()`、`panic()`、`cprintf()`、`init_memmap()`、`pa2page()`、链接符号 `end`、`struct Page` 及相关全局变量和宏。
- 设计提示词借助 AI 完成；报告写「功能模块」：最终提示词 + 实现迭代过程。
- 评测：`make grade` 的 `[Test 1] Page Init: PASS`（见 code/tools/test_lab2.c）。

## 当前进度 / 下一步
- 开始实现。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
