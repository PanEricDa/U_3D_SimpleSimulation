# 网格移动系统需求大纲 v2

## 1. 系统定位

- **风格**：类双点博物馆俯视角网格移动
- **规模约束**：地图上限 200×200 格（即 200m×200m 区域）
- **格子尺寸**：固定 100cm×100cm，不可配置
- **技术原则**：以 UE5.7 `NavigationSystem` + `NavMesh` 为避障底层，最小化自定义逻辑
- **集成方式**：单一 `UActorComponent`，附加到任意 `APawn` / `ACharacter` 即自动接管移动

---

## 2. 核心模块

### 2.1 GridMapSubsystem（地图数据 — WorldSubsystem）
- 全局单例，管理整张网格地图的生命周期
- 网格单元数据 `FGridCell`：
  - 逻辑坐标 `FIntPoint`
  - 世界坐标 `FVector`（含地表高度）
  - `bool bWalkable`（由 NavMesh 可达性决定）
  - `float MoveCost`（通行成本，默认 1.0）
  - `float SpeedMultiplier`（速度系数，默认 1.0）
  - `FGameplayTag MaterialTag`（材质规则匹配标签）
- 提供格坐标 ↔ 世界坐标互转 API
- 提供运行时修改单格属性的接口（动态障碍）

### 2.2 TerrainScanner（地形自动扫描）
- **触发时机**：`BeginPlay` 或手动调用 `RebuildGrid()`
- **扫描流程**：
  1. 以场景中标记为 `GridTerrain` Tag 的 `AStaticMeshActor` 的 AABB 为边界，按 100cm 步长划分逻辑格（上限 200×200）
  2. 每格中心向下 `LineTrace`（`ECC_WorldStatic`）采样地表高度与法线
  3. 坡度超过阈值（可配置）则标记不可通行
  4. **与 NavMesh 联动**：查询 `UNavigationSystemV1::GetNavigationData`，对每格中心点调用 `HasNodeAt` / `ProjectPoint` 验证该点是否在 NavMesh 上，不在则 `bWalkable = false`
  5. 读取地表材质，执行材质规则匹配，写入 `MoveCost` / `SpeedMultiplier` / `MaterialTag`

### 2.3 MaterialRuleSystem（材质规则）
- 配置资产 `UGridMaterialRuleDataAsset`（`UDataAsset` 子类），编辑器内维护规则表
- 每条规则：`材质匹配条件 → { bWalkable override, MoveCost, SpeedMultiplier }`
- 匹配方式：材质资产路径（软引用）或材质 Scalar 参数名+值范围
- 扫描时按优先级顺序匹配，首条命中规则生效

### 2.4 GridMovementComponent（移动组件）
- 继承 `UActorComponent`，`BeginPlay` 时自动查找 Owner 的 `UCharacterMovementComponent` 并将其 `MovementMode` 切换为 `MOVE_Custom`，接管位移控制
- **寻路**：调用 `UNavigationSystemV1::FindPathToLocationSynchronously`，获得 NavPath 后将路径点序列**映射到网格逻辑坐标**（就近取格），形成网格路径
- **8 方向移动**：对角线格视为合法相邻格，对角线移动成本 × √2 × `MoveCost`
- **移动执行**：按网格路径逐格 `FMath::VInterpTo` 插值，每格移动时长 = `CellSize / (BaseSpeed × SpeedMultiplier)`
- **动态避障**：监听 `UNavigationSystemV1::OnNavigationGenerationFinished`，NavMesh 更新后局部刷新受影响格的 `bWalkable`，并在行进中检测前方格可通行性，必要时重规划
- 移动状态机：`Idle → Pathfinding → Moving → Blocked → Arrived`，广播 Blueprint Delegate

### 2.5 Debug 可视化
- `bDebugDraw` 开关（运行时），使用 `DrawDebugHelpers` 绘制：
  - 可通行格（绿色线框）、不可通行格（红色）、当前路径（黄色）
  - 格子 MoveCost 数值悬浮文字

---

## 3. 配置接口（DataAsset）

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `SlopeThreshold` | `float` | `45.0` | 最大可通行坡度（度） |
| `TraceHeight` | `float` | `200.0` | 向下射线起点高于地面的距离（cm） |
| `BaseSpeed` | `float` | `300.0` | 基础移动速度（cm/s，即 3格/s） |
| `DiagonalRule` | `enum` | `Strict` | 对角线通行：Strict（两侧均可通行）/ Lenient / Disabled |
| `MaterialRules` | `UGridMaterialRuleDataAsset*` | `null` | 材质规则表 |
| `bDebugDraw` | `bool` | `false` | 调试可视化 |

---

## 4. Blueprint 公开 API

```
// 移动指令
MoveToCell(FIntPoint Target)
MoveToWorldLocation(FVector WorldPos)
StopMovement()

// 查询
FIntPoint GetCurrentCell()
FGridCell GetCellData(FIntPoint)
bool IsCellWalkable(FIntPoint)

// 运行时修改
SetCellWalkable(FIntPoint, bool)   // 动态障碍（临时覆盖）
RebuildGrid()                      // 重扫地形

// 事件 Delegate
OnCellEntered(FIntPoint, FGridCell)
OnMovementStateChanged(EGridMoveState)
OnPathBlocked(FIntPoint)
```

---

## 5. 与 UE5.7 原生系统集成映射

| 功能 | 依赖的原生系统 |
|------|--------------|
| 可通行性判断 | `UNavigationSystemV1::ProjectPoint` + `NavMesh` |
| 路径规划 | `UNavigationSystemV1::FindPathToLocationSynchronously` |
| NavMesh 变化感知 | `FNavigationSystem::OnNavigationGenerationFinished` |
| 地形碰撞采样 | `UWorld::LineTraceSingleByChannel` |
| 材质参数读取 | `UMaterialInterface::GetScalarParameterValue` |
| 移动插值 | `FMath::VInterpTo`（Tick 驱动）|
| 标签系统 | `UGameplayTagsManager`（`FGameplayTag`）|
| 配置序列化 | `UDataAsset` |

---

## 6. 明确不在范围内

- 多层网格（Z 轴叠层）
- 网络同步 / 多人联机
- 地形运行时动态变形自动感知
- 编辑器笔刷工具

---

## 7. 开发阶段与验证计划

> **原则**：全程只有一个测试场景 `GridTest`（用户自建地形），每阶段在其基础上累加内容。每阶段完成后须同时通过本阶段验收标准 **以及** 前序阶段的全部验收标准。所有 Debug 信息默认常显，无需手动开关。验收方式为用户手动观察。

### 进度总览

| 阶段 | 状态 | 备注 |
|------|------|------|
| P0 — 网格数据基础 + 地形扫描 | ✅ 用户验收通过 | TerrainScanner 障碍物高度 bug 已修复 |
| P1a — GridMovementComponent 核心 | ✅ 用户验收通过 | 逐格插值移动、NavMesh 寻路、状态机 |
| P1b — AGridTestCharacter AI 随机游走 | ✅ 用户验收通过 | AI 循环游走、卡死保护、ManualModeTimeout 自动回归 |
| P1c — Enhanced Input 点击交互 | ✅ 用户验收通过 | 重构为单 IA_GridClick + EGridControlMode 枚举状态机；已修复 trace 和模式切换 bug |
| P2 — 材质规则系统 | ✅ 用户验收通过 | |
| P3 — NavMesh 感知 + 预置障碍绕行 | ⏳ 待开始 | |

---

### 测试场景 `GridTest` — 累计布局规划

用户自建地形，各阶段逐步在同一场景中叠加内容，最终包含：

```
[ 平整地面（主区域）]  [ 斜坡区 ]
[ 静态障碍盒 x3     ]  [ 材质B 泥地区 ]
[ 材质C 禁区        ]  [ 编辑器预放障碍盒（P3 前放好）]
```

| 阶段 | 场景新增内容 |
|------|------------|
| P0 | 用户搭建地面 + 斜坡 + 静态障碍盒，放入 `AGridDebugActor` |
| P1a | `UGridMovementComponent` 核心，放入最简测试 Pawn 验证移动 |
| P1b | `AGridTestCharacter` AI 随机游走压力测试 |
| P1c | Enhanced Input 点击选中 / 手动移动 |
| P2 | 地面新增材质 B 区域（泥地）和材质 C 区域（禁区）|
| P3 | 编辑器内预先放置障碍盒（含 NavModifier），运行时验证网格正确更新并成功绕行 |

---

### P0 — 网格数据基础 + 地形扫描

**本阶段交付**
- `UGridMapSubsystem`（`UWorldSubsystem`）：`FGridCell` 数据结构 + 格坐标 ↔ 世界坐标互转 API
- `UTerrainScanner`：LineTrace 采样地表高度/法线，坡度过滤，NavMesh `ProjectPoint` 联动判通行
- `AGridDebugActor`：放入场景自动触发扫描，可安全删除，无业务依赖

**Debug 可视化**
- 每格 `DrawDebugBox` 常驻线框：
  - 可通行 → 绿色
  - NavMesh 不可达 → 红色
  - 坡度超限 → 橙色
- 屏幕固定区域（`AddOnScreenDebugMessage`，固定 Key 不堆叠）：
  ```
  [Grid] Cells: 400 | Walk: 312 | Blocked(Nav): 60 | Blocked(Slope): 28 | ScanTime: 12ms
  ```

**手动验收清单**
- [ ] 绿色格覆盖区域与编辑器 `Show Navigation` 可达区域目视吻合（误差 ≤ 1 格）
- [ ] 斜坡区格子显示橙色
- [ ] 静态障碍盒下方格子显示红色
- [ ] 屏幕 Log 中扫描耗时 < 100ms
- [ ] 删除 `AGridDebugActor` 后无残留线框、无报错

---

### P1a — GridMovementComponent 核心

**本阶段交付**（在 P0 基础上）
- `UGridMovementComponent`（`UActorComponent`）：
  - `BeginPlay` 自动接管 Owner 的 `UCharacterMovementComponent`（切换为 `MOVE_Custom`）
  - 调用 `UNavigationSystemV1::FindPathToLocationSynchronously` 获取 NavPath，映射为网格路径
  - 8 方向邻格 + 对角线规则（`DiagonalRule` 可配置：Strict / Lenient / Disabled）
  - 逐格 `VInterpTo` 插值移动，每步时长 = `CellSize / (BaseSpeed × SpeedMultiplier)`
  - 状态机：`Idle → Pathfinding → Moving → Arrived`
  - Blueprint Delegate：`OnCellEntered`、`OnMovementStateChanged`、`OnPathBlocked`
- 场景中放入一个最简 `ACharacter`（蓝图即可）挂载该组件，用 `MoveToWorldLocation` 手动在编辑器里触发（BeginPlay 写死目标坐标）做冒烟验证

**Debug 可视化**（叠加在 P0 之上）
- 当前路径：黄色 `DrawDebugDirectionalArrow`，逐格标注步骤序号
- 当前格：青色加粗线框
- 目标格：紫色加粗线框
- 屏幕新增（固定 Key）：
  ```
  [Move] State:Moving | Cell:(5,3)→(12,8) | Steps:9 | Progress:0.72 | Dir:NE
  [Move] NavPts:4 → GridPts:9 | Speed:300cm/s
  ```

**手动验收清单（含 P0 回归）**
- [ ] P0 全部验收标准仍然通过
- [ ] Pawn 从起点移动到写死目标，落点对齐格子中心，无漂移
- [ ] 路径经过静态障碍盒旁时正确绕行
- [ ] 对角线移动流畅无跳格
- [ ] 目标为不可通行格时 State 保持 `Idle`，屏幕显示 `[Move] Target not walkable`
- [ ] 到达目标后 State 变为 `Arrived`，路径线框消失

---

### P1b — AGridTestCharacter AI 随机游走

**本阶段交付**（在 P1a 基础上）
- `AGridTestCharacter`（`ACharacter` 子类）：
  - 挂载 `UGridMovementComponent`
  - `OnMovementStateChanged` → `Arrived` 时，从 `GridMapSubsystem` 随机抽取一个 `bWalkable=true` 的格作为下一目标，调用 `MoveToCell()`，持续循环
  - **卡死保护**：`Moving` 状态超过 `MaxMoveTimeout`（默认 10s）未到达，强制重选目标并在 Output Log 打印 `Warning`
  - `MaxMoveTimeout` 在组件上可配置
- 替换 P1a 的最简测试 Pawn，将 `AGridTestCharacter` 放入场景

**Debug 可视化**（叠加在 P1a 之上）
- 角色头顶 `DrawDebugString` 常驻显示：`[AI]`
- 屏幕新增：
  ```
  [Move] Mode:AI_Random | Target:(12,8) | Timeout:0/10s
  ```

**手动验收清单（含 P0、P1a 回归）**
- [ ] P0、P1a 全部验收标准仍然通过
- [ ] AI 角色自主循环游走，连续运行 **3 分钟**不卡死、不崩溃、不停在原地
- [ ] 每次到达目标后立即选取下一目标，无明显停顿（< 0.1s）
- [ ] 卡死保护触发时 Output Log 出现 `Warning`，角色重新开始移动
- [ ] 整个过程屏幕无报错红字

---

### P1c — Enhanced Input 点击交互

**本阶段交付**（在 P1b 基础上）
- `AGridTestPlayerController`：运行时加载 `IMC_Grid`（`UInputMappingContext`），读取 `UGridInputConfig` 持有的 InputAction 引用
- `UGridInputConfig`（`UDataAsset`）：持有所有 `UInputAction` 软引用，供 PlayerController 统一读取
- 交互逻辑：
  - `IA_GridSelect`（左键单击）→ 射线检测：命中 `AGridTestCharacter` 则选中并暂停 AI；命中其他位置则不处理
  - `IA_GridMoveTo`（左键单击，仅选中状态生效）→ 射线检测命中地面 → `MoveToWorldLocation(HitLocation)`
  - `IA_GridDeselect`（右键单击）→ 取消选中，恢复 AI 随机游走
- 选中时：当前格高亮保持青色，头顶文字切换为 `[Manual]`

**Debug 可视化**（叠加在 P1b 之上）
- 角色头顶文字按模式切换：`[AI]` / `[Manual]`
- 屏幕 `Mode` 行同步更新：
  ```
  [Move] Mode:Manual | Target:(8,5)
  [Move] Mode:AI_Random | Target:(12,8)
  ```

**手动验收清单（含 P0、P1a、P1b 回归）**
- [ ] P0、P1a、P1b 全部验收标准仍然通过
- [ ] 左键点击 AI 角色：角色停止，头顶显示 `[Manual]`，屏幕显示 `Mode:Manual`
- [ ] 选中后左键点击可通行地面：角色移动到目标格，路径可见
- [ ] 选中后左键点击不可通行格：不移动，屏幕显示 `[Move] Target not walkable`
- [ ] 右键取消选中：恢复 AI，头顶显示 `[AI]`，屏幕显示 `Mode:AI_Random`
- [ ] 未选中状态下左键点击地面：无任何响应（不误触移动）

---

### P2 — 材质规则系统

**本阶段交付**（在 P1 基础上累加）
- `UGridMaterialRuleDataAsset`（`UDataAsset` 子类）：编辑器内配置规则表
  - 每条规则：`材质软引用 或 Scalar参数名+值范围 → { bWalkable, MoveCost, SpeedMultiplier }`
  - 按优先级排序，首条命中生效
- 材质匹配逻辑集成进 `UTerrainScanner`，扫描时写入 `FGridCell`
- 场景内地面新增：
  - 材质 B 区域（泥地）：`MoveCost=2.0, SpeedMultiplier=0.5`
  - 材质 C 区域（禁区）：`bWalkable=false`

**Debug 可视化**（叠加在 P1 之上）
- 格子线框按 `MoveCost` 渐变：`1.0`=绿，`1.5`=黄，`≥2.0`=红
- 材质覆写不可通行格：深红色线框 + DrawDebugLine 画 X
- 非默认属性格显示悬浮文字（`DrawDebugString`）：`C:2.0 S:0.5`
- 屏幕新增（固定 Key）：
  ```
  [MatRule] Matched:87 | Walk↓:23 | Cost↑:41 | Speed↓:23 | Default:225
  [Cell] (8,4) Mat:Mud | Cost:2.0 | SpeedMul:x0.5 | Walk:true   ← 角色踏入时刷新
  ```

**手动验收清单（含 P0、P1a~P1c 回归）**
- [ ] P0、P1a、P1b、P1c 全部验收标准仍然通过
- [ ] 材质 B 区域格显示红色线框 + `C:2.0 S:0.5` 文字
- [ ] 材质 C 区域格显示深红 + X，AI 角色绕行不进入
- [ ] AI 或手动操控角色进入材质 B 区域：屏幕 `[Cell]` 行实时更新，角色视觉速度明显变慢
- [ ] 离开材质 B 区域：速度恢复，`[Cell]` 行更新为默认值
- [ ] 修改 DataAsset 规则后调用 `RebuildGrid()` → 格子颜色立即刷新，无需重启

---

### P3 — NavMesh 感知 + 预置障碍绕行验证

**本阶段交付**（在 P2 基础上累加）
- 监听 `UNavigationSystemV1::OnNavigationGenerationFinished`，游戏启动时 NavMesh 完成构建后自动触发 `TerrainScanner` 扫描（确保障碍盒已计入 NavMesh 后再生成网格数据）
- 行进中每步出发前检测目标格 `bWalkable`，若路径中断则自动重规划；无路可走则进入 `Blocked` 状态并广播 `OnPathBlocked`
- `SetCellWalkable(FIntPoint, bool)` API 保留（供后续扩展，本阶段不测试运行时调用）
- **场景准备（运行前由用户操作）**：在编辑器内于 AI 游走路径上放置若干带 `UNavModifierComponent`（Area Class = `NavArea_Null`）的障碍盒，确保障碍盒遮挡了原来的直线路径，留有绕行空间

**Debug 可视化**（叠加在 P2 之上）
- 障碍盒占据格在扫描完成后立即显示红色线框（与静态障碍盒一致）
- 屏幕新增（固定 Key）：
  ```
  [Nav]  NavMesh ready → triggering scan
  [Grid] Rescan done | Walk:298 (was 312, -14 blocked by obstacles)
  [Move] Replanning → OK | new:11 steps (was 7)
  [Move] State:Blocked | No path to (12,8)    ← 无路可走时常驻
  ```

**手动验收清单（含 P0、P1a~P1c、P2 回归）**
- [ ] P0、P1a、P1b、P1c、P2 全部验收标准仍然通过
- [ ] 运行后屏幕显示 `NavMesh ready → triggering scan`，扫描数据更新（Walkable 数量减少）
- [ ] 障碍盒占据格显示红色线框，与编辑器内 `Show Navigation` 不可达区域吻合
- [ ] AI 角色游走路径成功绕过所有预置障碍盒，不穿入、不卡死
- [ ] 手动模式下点击障碍盒另一侧的目标格：角色成功规划绕行路径并到达
- [ ] 目标被完全封堵时：屏幕显示 `State:Blocked`，角色原地停止，不循环报错
- [ ] Output Log 无 Error / Warning

---

### Debug 视觉规范（全阶段统一）

| 元素 | 绘制方式 | 颜色 / 备注 |
|------|---------|------------|
| 可通行格 | `DrawDebugBox` | 绿 |
| NavMesh 不可达 | `DrawDebugBox` | 红 |
| 坡度超限 | `DrawDebugBox` | 橙 |
| 材质覆写不可通行 | `DrawDebugBox` + X | 深红 |
| MoveCost 渐变格 | `DrawDebugBox` | 绿→黄→红 |
| 当前路径 | `DrawDebugDirectionalArrow` | 黄，含步骤序号 |
| 失效旧路径 | `DrawDebugLine` | 灰，0.5s 消失 |
| 当前格 | `DrawDebugBox`（加粗） | 青 |
| 目标格 | `DrawDebugBox`（加粗） | 紫 |
| 格属性文字 | `DrawDebugString` | 白，仅非默认格 |
| 角色模式标识 | `DrawDebugString`（头顶） | 白，`[AI]` / `[Manual]` |
| 状态刷新闪烁 | `DrawDebugBox` | 白，0.5s |
| 屏幕 Log | `AddOnScreenDebugMessage`（固定 Key） | 分模块，不堆叠 |

---

### Enhanced Input 资产清单（P1c 交付）

| 资产 | 类型 | 说明 |
|------|------|------|
| `IA_GridSelect` | `UInputAction` | 左键单击，命中角色则选中并暂停 AI |
| `IA_GridMoveTo` | `UInputAction` | 选中状态下左键单击地面，手动指定移动目标 |
| `IA_GridDeselect` | `UInputAction` | 右键单击，取消选中，恢复 AI |
| `IMC_Grid` | `UInputMappingContext` | 统一映射上下文，`BeginPlay` 时加载 |
| `UGridInputConfig` | `UDataAsset` | 持有以上 InputAction 软引用，供 PlayerController 统一读取 |
