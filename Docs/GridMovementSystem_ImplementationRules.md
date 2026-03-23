# 网格移动系统 — 实现规则与工作流

## 1. 总体原则

- 严格按阶段顺序推进：P0 → P1a → P1b → P1c → P2 → P3
- 每个阶段内，按**类为单位**逐个实现，不跨类并行编写
- 每个类编写完成后必须通过编译验证，方可开始下一个类
- 阶段内所有类编译通过后，进行编辑器配置指导，再由用户手动验收

---

## 2. 单类工作流

每个类的完整处理流程如下：

```
[Code Writer Agent]
  └─ 按需求编写该类的 .h 和 .cpp
        │
        ▼
[Code Reviewer Agent]
  └─ 执行项目编译
  └─ 检查 Output Log 中的 Error / Warning
  └─ 若有编译错误 → 自动修正 → 重新编译，直至编译成功
        │
        ▼
[通知用户]
  └─ 报告该类编译通过
  └─ 简述该类的职责与对外接口
  └─ 询问是否继续下一个类
```

> **禁止跳步**：Code Writer Agent 不得在上一个类编译通过前开始编写下一个类。

---

## 3. Agent 职责边界

### Code Writer Agent
- 负责根据需求文档编写 C++ 代码（`.h` + `.cpp`）
- 遵循 UE5.7 编码规范：UObject 命名前缀、`UPROPERTY` / `UFUNCTION` 标记、模块依赖声明
- 每次只编写**一个类**，不超范围
- 代码中所有 Debug 可视化（`DrawDebugBox`、`AddOnScreenDebugMessage` 等）默认开启，无需手动切换

### Code Reviewer Agent
- 负责触发 UE5 项目编译（`UnrealBuildTool`）
- 读取编译输出，识别并定位所有 Error / Warning
- 对编译错误进行修正并重新触发编译，循环直至 0 Error
- Warning 视情况修正，不阻塞流程但需记录
- 编译通过后输出简报：通过的类名、修正内容摘要（若有）

---

## 4. 阶段完成后的交付规范

每个阶段（P0 / P1a / P1b / P1c / P2 / P3）所有类编译通过后，须向用户提供：

### 4.1 编辑器配置步骤
以步骤编号列出，内容包括：
- 需要在场景中放置哪些 Actor，放置位置建议
- 需要创建哪些资产（DataAsset、InputAction、MappingContext 等），在哪个目录创建
- 需要设置哪些属性值（Class Default、Details 面板参数）
- 需要启用哪些引擎设置（如 NavigationSystem、Enhanced Input Plugin）
- 需要给哪些 Actor / Mesh 添加什么 Tag 或组件

### 4.2 功能说明
- **实现了什么**：本阶段新增了哪些能力，用一两句话概括
- **如何实现的**：核心机制的简要说明（使用了哪些 UE5 原生系统、关键调用链）
- **与前序阶段的关系**：本阶段依赖前序哪些类，扩展了哪些接口

### 4.3 验收提示
- 列出本阶段的手动验收清单（摘自需求文档）
- 提示用户运行场景后重点观察哪些 Debug 视觉元素

---

## 5. 编译规范要求

- 项目模块：所有新类放在项目主模块下，不新建独立插件模块
- 头文件包含：只引入必要头文件，使用前向声明减少耦合
- 模块依赖（`Build.cs`）：按需添加，至少包含：
  ```
  "NavigationSystem", "AIModule", "GameplayTags", "EnhancedInput"
  ```
- 不使用 `#pragma once` 以外的非标准宏
- 所有暴露给 Blueprint 的属性加 `EditAnywhere` / `BlueprintReadWrite`，所有暴露给 Blueprint 的函数加 `BlueprintCallable`

---

## 6. 阶段与类清单（实现顺序）

| 阶段 | 类名 | 类型 | 依赖 |
|------|------|------|------|
| P0-1 | `FGridCell` | Struct | — |
| P0-2 | `UGridMapSubsystem` | `UWorldSubsystem` | `FGridCell` |
| P0-3 | `UTerrainScanner` | `UObject` | `UGridMapSubsystem` |
| P0-4 | `AGridDebugActor` | `AActor` | `UGridMapSubsystem` |
| P1a-1 | `UGridConfig` | `UDataAsset` | — |
| P1a-2 | `UGridMovementComponent` | `UActorComponent` | `UGridMapSubsystem`、`UGridConfig` |
| P1b-1 | `AGridTestCharacter` | `ACharacter` | `UGridMovementComponent` |
| P1c-1 | `UGridInputConfig` | `UDataAsset` | — |
| P1c-2 | `AGridTestPlayerController` | `APlayerController` | `UGridInputConfig`、`AGridTestCharacter` |
| P2-1 | `FGridMaterialRule` | Struct | — |
| P2-2 | `UGridMaterialRuleDataAsset` | `UDataAsset` | `FGridMaterialRule` |
| P2-3 | （扩展）`UTerrainScanner` | 修改已有类 | `UGridMaterialRuleDataAsset` |
| P3-1 | （扩展）`UGridMapSubsystem` | 修改已有类 | `UNavigationSystemV1` |
| P3-2 | （扩展）`UGridMovementComponent` | 修改已有类 | `UGridMapSubsystem` |

> 标注"修改已有类"的条目不新建文件，在原类上扩展，同样需要独立编译验证后再进行下一条。

---

## 7. 异常处理规则

| 情况 | 处理方式 |
|------|---------|
| 编译 Error | Code Reviewer Agent 自动修正，不通知用户，直至通过 |
| 编译 Warning（UE 标准 Warning）| 记录在简报中，不阻塞 |
| 编译 Warning（逻辑隐患）| 修正后再通过 |
| 需求与实现产生冲突 | 暂停，向用户说明冲突点并询问决策，不自行假设 |
| 用户验收未通过 | 回到该阶段对应类进行修正，重走单类工作流 |
