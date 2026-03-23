// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GridTypes.h"
#include "GridConfig.h"
#include "GridMovementComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// 前向声明
// ─────────────────────────────────────────────────────────────────────────────
class UGridMapSubsystem;

// ─────────────────────────────────────────────────────────────────────────────
// 移动状态枚举
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @enum EGridMoveState
 * @brief 网格移动组件的运行时状态机枚举
 * @note 状态转换顺序：Idle → Pathfinding → Moving → Arrived/Blocked
 */
UENUM(BlueprintType)
enum class EGridMoveState : uint8
{
    /** 静止，无移动指令 */
    Idle        UMETA(DisplayName = "Idle"),

    /** 正在执行寻路计算 */
    Pathfinding UMETA(DisplayName = "Pathfinding"),

    /** 正在沿路径逐格插值移动 */
    Moving      UMETA(DisplayName = "Moving"),

    /** 已到达目标格 */
    Arrived     UMETA(DisplayName = "Arrived"),

    /** 路径被动态障碍物阻断 */
    Blocked     UMETA(DisplayName = "Blocked")
};

// ─────────────────────────────────────────────────────────────────────────────
// Delegate 声明（放在类外，确保蓝图可绑定）
// ─────────────────────────────────────────────────────────────────────────────

/** 进入新格子时触发，携带格坐标与完整格子数据 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCellEntered, FIntPoint, Cell, FGridCell, CellData);

/** 移动状态发生变化时触发 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveStateChanged, EGridMoveState, NewState);

/** 路径被障碍物阻断时触发，携带阻断格坐标 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPathBlocked, FIntPoint, BlockedCell);

// ─────────────────────────────────────────────────────────────────────────────
// 组件主类
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class UGridMovementComponent
 * @brief 网格移动组件 - 驱动 Actor 在网格地图上进行基于寻路的逐格插值移动
 *
 * 工作流程：
 *   1. 调用 MoveToCell / MoveToWorldLocation 触发寻路
 *   2. BuildGridPath 通过 NavMesh 生成路径并映射到网格坐标序列
 *   3. TickComponent 逐步推进插值，每完成一格调用 StartNextStep
 *   4. 到达终点后触发 OnCellEntered / OnMovementStateChanged(Arrived)
 *
 * @note 本组件挂载到 ACharacter 时会将 CharacterMovementComponent 切换为
 *       MOVE_Custom 模式，由本组件完全接管位移逻辑，避免两套系统冲突。
 */
UCLASS(ClassGroup=(Grid), meta=(BlueprintSpawnableComponent))
class U3D_SIMPLE_API UGridMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UGridMovementComponent();

    // ─────────────────────────────────────────────────────────────────────────
    // 配置
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 可选配置资产
     * @note 留空时组件使用内置默认值：BaseSpeed=300.f，DiagonalRule=Strict
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Movement")
    TObjectPtr<UGridConfig> Config;

    // ─────────────────────────────────────────────────────────────────────────
    // 事件代理
    // ─────────────────────────────────────────────────────────────────────────

    /** 进入新格子时广播（每走完一格触发一次） */
    UPROPERTY(BlueprintAssignable, Category = "Grid|Events")
    FOnCellEntered OnCellEntered;

    /** 移动状态变化时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Grid|Events")
    FOnMoveStateChanged OnMovementStateChanged;

    /** 路径被阻断时广播（动态障碍检测） */
    UPROPERTY(BlueprintAssignable, Category = "Grid|Events")
    FOnPathBlocked OnPathBlocked;

    // ─────────────────────────────────────────────────────────────────────────
    // Blueprint API
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 移动到指定网格坐标
     * @param TargetCell 目标格逻辑坐标（列, 行）
     * @note 若目标格不可通行或无法寻路，触发 OnPathBlocked 并切换至 Blocked 状态
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void MoveToCell(FIntPoint TargetCell);

    /**
     * @brief 移动到指定世界坐标所在的格子
     * @param WorldPos 目标世界坐标（自动映射至最近网格格子）
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void MoveToWorldLocation(FVector WorldPos);

    /**
     * @brief 立即停止移动，清空路径，切换至 Idle 状态
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void StopMovement();

    /**
     * @brief 吸附到最近格中心后停止，用于手动选中时的精确定位
     * @note  若当前正在移动且步进 Alpha >= 0.5，吸附到下一格；否则吸附到当前格
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void StopAtNearestCell();

    /**
     * @brief 获取 Actor 当前所在格坐标
     * @return 当前格逻辑坐标（列, 行）
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FIntPoint GetCurrentCell() const;

    /**
     * @brief 查询指定格子的完整数据
     * @param Coord    目标格逻辑坐标
     * @param OutCell  [out] 查询成功时填充格子数据
     * @return 格子存在返回 true，否则返回 false
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool GetCellData(FIntPoint Coord, FGridCell& OutCell) const;

    /**
     * @brief 快速查询指定格子是否可通行
     * @param Coord 目标格逻辑坐标
     * @return 可通行返回 true
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool IsCellWalkable(FIntPoint Coord) const;

    /**
     * @brief 获取当前移动状态
     * @return 当前 EGridMoveState 枚举值
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    EGridMoveState GetMoveState() const;

    /**
     * @brief Returns the current movement destination cell.
     * @note  Only meaningful when MoveState is Moving or Pathfinding.
     */
    UFUNCTION(BlueprintCallable, Category = "Grid")
    FIntPoint GetTargetCell() const;

    // ─────────────────────────────────────────────────────────────────────────
    // UActorComponent 覆写
    // ─────────────────────────────────────────────────────────────────────────

    /** 初始化：缓存 CMC、接管移动模式、记录起始格坐标 */
    virtual void BeginPlay() override;

    /**
     * @brief 逐帧驱动插值移动与 Debug 可视化
     * @param DeltaTime           本帧时间步长（秒）
     * @param TickType            Tick 类型
     * @param ThisTickFunction    本组件的 Tick 函数引用
     */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

private:

    // ─────────────────────────────────────────────────────────────────────────
    // 运行时状态
    // ─────────────────────────────────────────────────────────────────────────

    /** 当前移动状态，通过 SetMoveState 统一修改以触发代理广播 */
    EGridMoveState MoveState = EGridMoveState::Idle;

    /** Actor 当前所在格的逻辑坐标 */
    FIntPoint CurrentCell;

    /** 本次移动的目标格逻辑坐标 */
    FIntPoint TargetCell;

    /** 寻路生成的完整格坐标路径（不含起始格） */
    TArray<FIntPoint> GridPath;

    /** 当前正在执行的步骤索引（对应 GridPath 中的位置） */
    int32 PathIndex = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // 单步插值数据
    // ─────────────────────────────────────────────────────────────────────────

    /** 本步插值起始世界坐标（Actor 踏上该步时的精确位置） */
    FVector StepStartPos;

    /** 本步插值目标世界坐标（下一格的格心世界坐标） */
    FVector StepTargetPos;

    /** 插值进度，范围 [0, 1]；到达 1.0 时触发到格逻辑 */
    float StepAlpha = 0.f;

    /** 本步所需时长（秒），由格子尺寸、速度与地形倍率共同决定 */
    float StepDuration = 0.f;

    // ─────────────────────────────────────────────────────────────────────────
    // 缓存
    // ─────────────────────────────────────────────────────────────────────────

    /** Owner Character 的移动组件弱引用，仅用于 BeginPlay 时将其完全禁用 */
    TWeakObjectPtr<UCharacterMovementComponent> CachedCMC;

    /** 角色胶囊半高（cm），BeginPlay 时缓存，用于将格子地表 Z 转换为角色中心 Z */
    float CachedZOffset = 0.f;

    // ─────────────────────────────────────────────────────────────────────────
    // 私有函数
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 统一修改移动状态并广播事件
     * @param NewState 新状态；与当前状态相同时提前返回，避免冗余广播
     */
    void SetMoveState(EGridMoveState NewState);

    /**
     * @brief 通过 NavMesh 生成路径并将路径点映射为网格坐标序列
     * @param FromWorld 起始世界坐标
     * @param ToWorld   目标世界坐标
     * @param OutPath   [out] 生成的格坐标路径（不含起始格）
     * @return 成功生成非空路径返回 true
     */
    bool BuildGridPath(const FVector& FromWorld, const FVector& ToWorld,
                       TArray<FIntPoint>& OutPath);

    /**
     * @brief 启动下一步插值（从 GridPath[PathIndex] 开始）
     * @note 会检查目标格的动态通行性，若被阻断则切换 Blocked 状态
     */
    void StartNextStep();

    /**
     * @brief 完成全部路径移动，对齐到终点格中心并广播 Arrived
     */
    void FinishMovement();

    /**
     * @brief 将两格之间的相对方向转为方向字符串（用于 Debug 显示）
     * @param From 起始格坐标
     * @param To   目标格坐标
     * @return "N"/"S"/"E"/"W"/"NE"/"NW"/"SE"/"SW" 或 "?" 表示原地
     */
    FString GetDirectionString(const FIntPoint& From, const FIntPoint& To) const;

    /**
     * @brief 每帧绘制调试信息（当前格、目标格、路径箭头、屏幕日志）
     * @note bPersistentLines=false，LifeTime=-1，每帧自动刷新，不与 P0 的持久格子叠加
     */
    void DrawDebugInfo() const;

    /**
     * @brief 获取基础移动速度（cm/s）
     * @return Config->BaseSpeed；Config 为 null 时返回 300.f
     */
    float GetBaseSpeed() const;

    /**
     * @brief 获取对角线移动规则
     * @return Config->DiagonalRule；Config 为 null 时返回 EGridDiagonalRule::Strict
     */
    EGridDiagonalRule GetDiagonalRule() const;

    /**
     * @brief 判断两格之间的对角线移动是否被允许
     * @param From 起始格坐标
     * @param To   目标格坐标（必须与 From 斜角相邻）
     * @param Sub  网格子系统指针
     * @return 当前版本恒返回 true（NavMesh 已处理障碍），留作后续扩展
     */
    bool IsDiagonalAllowed(const FIntPoint& From, const FIntPoint& To,
                           const UGridMapSubsystem* Sub) const;
};
