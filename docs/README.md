# Ollie 项目文档

| 文档 | 内容 | 什么时候看 |
|---|---|---|
| [requirements.md](requirements.md) | 需求规格：功能需求编号、验收标准、范围边界 | 加功能前、对"该不该做"有疑问时 |
| [tech-spec.md](tech-spec.md) | 技术规格：BOM、引脚分配、模块规格、性能预算、依赖版本 | 接线、改硬件、评估改动影响时 |
| [design-conventions.md](design-conventions.md) | 设计规范：代码风格、命名、架构约束、Git 与文档规矩 | 写任何代码之前 |
| [procedures.md](procedures.md) | 执行步骤：构建、三层测试、标定、排查、验收的操作规程 | 动手干活时照着做 |
| [work-guide.md](work-guide.md) | 工作说明：环境搭建、如何加功能、如何调试、常见坑 | 新会话/新人上手第一篇 |
| [roadmap.md](roadmap.md) | 迭代路线图：v1.1→v2.2 各版本内容、实现方式、验收与排布 | 看接下来做什么、排优先级时 |
| [plan-v1.md](plan-v1.md) | v1 可行性评定与实施计划（立项文档，含里程碑 M0–M7） | 了解项目全貌和决策背景 |

## 文档规矩（全项目适用）

1. **单一事实源**：引脚/阈值/节奏以 `Config.h` 为准，性能数据以最近一次实测为准，文档与代码冲突时先改齐再提交
2. **改动同步义务**：改引脚 → 同步 tech-spec 引脚表；改验收标准 → 同步 requirements；改流程 → 同步 procedures
3. 开发过程记录在 [devlog/](../devlog/README.md)，进行中任务只看 [devlog/TODO.md](../devlog/TODO.md)
