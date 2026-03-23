# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 工作流规则（必须遵守）

本项目正在开发网格移动系统，**所有实现工作须严格遵守 `Docs/GridMovementSystem_ImplementationRules.md`**，核心规则：

- **每次只编写一个类**，编译通过后方可开始下一个类
- Code Writer Agent 负责编写代码，Code Reviewer Agent 负责编译验证和纠错
- 阶段完成后须向用户提供编辑器配置步骤 + 功能说明 + 验收提示
- 需求与实现产生冲突时，暂停并询问用户，不自行假设
- **All output must be in English**

## 编译命令

```bash
# Editor 目标（开发中最常用）
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" U3D_SimpleEditor Win64 Development "I:/UE5Project/U3D_Simple/U3D_Simple.uproject" -waitmutex

# Game 目标
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" U3D_Simple Win64 Development "I:/UE5Project/U3D_Simple/U3D_Simple.uproject" -waitmutex

# 仅重新生成项目文件（修改 .Build.cs 后执行）
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/GenerateProjectFiles.bat" "I:/UE5Project/U3D_Simple/U3D_Simple.uproject" -game
```

编译成功标志：Output 末尾出现 `BUILD SUCCESSFUL`，无 `error` 行。

## 项目结构

```
U3D_Simple/
├── Source/
│   ├── U3D_Simple/
│   │   ├── U3D_Simple.Build.cs   # 模块依赖配置，新增依赖在此声明
│   │   ├── U3D_Simple.h          # 模块头（仅含 CoreMinimal.h）
│   │   └── U3D_Simple.cpp        # 模块入口（IMPLEMENT_PRIMARY_GAME_MODULE）
│   ├── U3D_Simple.Target.cs      # Game 目标，BuildSettingsVersion.V6
│   └── U3D_SimpleEditor.Target.cs # Editor 目标
├── Config/
│   ├── DefaultInput.ini          # 已配置 EnhancedInput 为默认输入系统
│   └── DefaultGame.ini
├── Content/
│   └── StartLite/                # 初始示例资产（与网格系统无关）
├── Docs/
│   ├── GridMovementSystem_Requirements.md      # 系统需求与验收清单
│   └── GridMovementSystem_ImplementationRules.md # 实现工作流规则
└── U3D_Simple.uproject           # UE 5.7，单模块 Runtime
```

所有新增的网格系统 C++ 类统一放在 `Source/U3D_Simple/` 下，**不新建子插件模块**。

## 模块依赖

当前 `U3D_Simple.Build.cs` 已包含：

| 类型 | 模块 |
|------|------|
| Public | `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `PhysicsCore` |
| Private | `RenderCore` |

网格系统还需要以下模块，**在开始 P0 实现前需添加到 Build.cs**：

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "NavigationSystem",
    "AIModule",
    "GameplayTags"
});
```

## Enhanced Input 配置状态

`DefaultInput.ini` 已将全局输入系统切换为 Enhanced Input：

```ini
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

P1c 阶段创建的 `IMC_Grid` 和 `IA_*` 资产无需再修改 ini，直接在 PlayerController 中加载即可。

## 网格系统开发进度

实现顺序与当前阶段见 `Docs/GridMovementSystem_ImplementationRules.md` 第 6 节"阶段与类清单"。

测试场景为单一累积场景 `GridTest`（用户自建地形），各阶段在同一场景叠加内容。

## 代码规范

- 所有新类加 `U3D_SIMPLE_API` 宏导出
- `UPROPERTY` 暴露给编辑器用 `EditAnywhere, BlueprintReadWrite, Category="Grid"`
- `UFUNCTION` 暴露给蓝图用 `BlueprintCallable, Category="Grid"`
- Debug 绘制（`DrawDebugBox`、`AddOnScreenDebugMessage`）默认常开，无需运行时开关
- 屏幕 Log 使用固定整数 Key（各模块不重叠），防止同类信息堆叠
