# 目录结构规范

这是一个多人协作的repo，同时作为实验交付产品

## 助教要求如下

* 一个分支对应一次实验，并以“labx”命名（例如：lab1），每一个分支中至少两个文件夹，分别以“code”和“report”命名。
* code文件夹包含对应实验的实现后的代码
* report文件夹中包含实验报告，以”report.md“命名 (嵌入的图片放在同级images/下) ，具体格式参考report-template.md、提示词文件以“prompt.md”命名，汇总本实验所有的prompt

## 本组约定

```
labx 分支
├── code/                     # 实验代码
├── report/
│   ├── report.md             # 最终报告
│   ├── prompt.md             # 提示词汇总
│   ├── sections/             # 各任务的报告章节，见下方「报告分 section 撰写」
│   └── images/               # 报告图片
└── .handoff/                 # 协作状态，见下方「.handoff/ 协作规范」
```



# 课程资源

该课程提供了网页教程以及Q&A答疑平台等资源

* 实验指导书网址：  http://8.135.34.58/lab2026/_book/ ，经常更新，可以使用claude-in-chrome查看。**通常实验要求就在指导书中。**
* 实验答疑平台： https://nankai.feishu.cn/docx/VgvqdhoIxotMuBxaYdWc6NhDnWg?from=from_copylink 可以尝试使用飞书CLI进行访问。
* 其他课程动态通常在微信群聊中发布，可以询问用户。



# 角色与分工

* 成员：**openfar**、**lyp**、**nagilix**。每个人及其 agent 都以该成员的身份工作。
* **openfar **：创建 lab 分支并初始化 `.handoff/`，指派任务，审核任务，把各 section 合并进 `report.md` 和 `prompt.md`，负责最终的 `make grade` 验证。
* **任务一律由 openfar 指派**。其他成员和 agent 不能自行认领或改派任务；有异议时写在对应任务文件的「留言」区。
* 通过一个 agent 调用另一个 agent（例如在 Claude Code 里调用 Codex）时，被调用方继承调用者的身份和任务范围，同样遵守本规范。



# Git 提交规范

* 每一个lab过程中，所有成员/agent在当前的labx分支中工作。
* 每个 labx 分支都从 main 切出。main 只保存项目规范和模板。
* 提交信息以任务 ID 开头，例如 `T2: 补充 GDB 跟踪截图`；只改 `.handoff/` 的提交写成 `handoff: ...`。
* push 前先 `git pull --rebase`。不能用覆盖他人改动的方式解决冲突，拿不准的地方要保守处理，并在留言区说明。



# .handoff/ 协作规范

`.handoff/` 是所有人和 agent 共享的状态目录，里面只保存**当前有用的状态**，包括目标、指派、进度、决策、依赖接口、验证结果、阻塞项和下一步。不要在这里写流水账或大段推理过程。

**每个 labx 分支单独维护 `.handoff/`，不继承 main 或上一个 lab 的内容。**

```
.handoff/
├── STATUS.md              # 本 lab 总览，是唯一由多人共同编辑的文件
└── tasks/
    └── T<n>-<短名>.md     # 一个任务一个文件，例如 T2-gdb-boot.md
```

## STATUS.md

```markdown
# Lab<N> 状态    截止：YYYY-MM-DD    集成负责人：openfar

## 目标
（1-3 行，概括本 lab 要交付的内容）

## 任务看板
| ID | 任务 | 负责人 | 状态 | 可改文件 |
|----|------|--------|------|----------|
| T1 | ... | lyp | 进行中 | code/kern/init/entry.S, report/sections/T1-*.md |

## 集成状态
- make qemu ⬜   make grade ⬜   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
（不超过 5 条，例如公共接口变更、环境问题。过时的条目要删掉）
```

* 只修改**自己任务那一行的「状态」列**，需要时可以在「全组须知」里增加条目。
* 状态只有这几种：`待开始` / `进行中` / `阻塞` / `待审` / `完成`。

## tasks/T\<n\>-\<短名\>.md

```markdown
# T<n> <任务名>
- 负责人：xxx    状态：进行中    依赖：T1（如有）
- 可改文件：（与看板一致）

## 要求
（指导书原文或 openfar 的指派说明）

## 当前进度 / 下一步

## 关键决策与结论
（包括其他任务可能依赖的接口和文件）

## 验证结果
（命令、结果摘要、截图文件名）

## 提示词记录
### v1
提示词原文：……
结果 / 问题：……
修改思路：……
### v2（最终）
……

## 留言
（非负责人只能在这里追加，每条格式为 `- [名字 日期] 内容`）
```

* **「可改文件」起到文件锁的作用**：只能修改自己任务名下的文件。如果必须改动别人的文件，先在对方任务文件的留言区说明。
* 「提示词记录」里的提示词必须保留原文，每轮迭代都要记录。它是报告中「最终提示词 / 实现迭代过程」和 `prompt.md` 的原始素材，事后很难补全。



# 报告分 section 撰写

* 每个任务的负责人把报告内容写在 `report/sections/T<n>-<短名>.md`，格式按 `report-template.md` 中对应的部分来写，例如「功能模块」「练习」或「Challenge」。
* 实验目的、整体逻辑、测试与验证、实验总结这类全组共享的章节，也作为任务指派给某个人，写法相同。
* 图片放在 `report/images/`，文件名以任务 ID 开头（例如 `T2-break-0x80200000.png`）。section 里的引用路径按 report.md 的位置来写（`./images/xxx.png`），这样合并时可以直接复制。
* `report.md` 和 `prompt.md` 由 openfar 合并编辑。



# 工作流

## 每次 session 启动时

1. 确认你的角色（openfar、lyp 或 nagilix）。如果提示词中没有说明，询问用户。
2. 切换到当前 labx 分支，执行 `git status` 检查同步状态，然后 `git pull --rebase` 拉取更新并处理冲突。
3. 阅读 `.handoff/STATUS.md` 和自己名下的任务文件，再开始工作。

## 工作中和 session 结束前

* 每取得一次有意义的进展，以及每次 session 结束前，都要更新自己的任务文件（进度、下一步、验证结果、提示词记录）和看板上自己的状态，然后 commit 并 push。
* 修改了其他任务依赖的接口或文件时，要在「全组须知」里写明。
* 任务完成后把状态改为 `待审`，由 openfar 审核后改为 `完成`。
* 测试（`make qemu` / `make grade`）没有通过的代码，不能标记为待审，也不能合并进最终版本。



# Others

* 此项目中使用简体中文和用户对话，撰写报告，写注释。
