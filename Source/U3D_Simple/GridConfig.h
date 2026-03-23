// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GridConfig.generated.h"

/**
 * @brief 对角线移动规则枚举
 *
 * 控制网格寻路时对角线方向的通行判断策略。
 */
UENUM(BlueprintType)
enum class EGridDiagonalRule : uint8
{
    Strict   UMETA(DisplayName = "Strict (Both sides walkable)"),   // 两侧格均可通行才允许对角线
    Lenient  UMETA(DisplayName = "Lenient (One side walkable)"),    // 至少一侧可通行即允许
    Disabled UMETA(DisplayName = "Disabled")                        // 禁用对角线移动
};

/**
 * @class UGridConfig
 * @brief 网格移动配置资产 - 存储网格移动系统的全局参数
 *
 * 以 DataAsset 形式存在，可在编辑器中创建多套配置并按需切换。
 * 挂载到 GridMovementComponent 或 GameMode 中引用即可生效。
 */
UCLASS(BlueprintType)
class U3D_SIMPLE_API UGridConfig : public UDataAsset
{
    GENERATED_BODY()

public:

    /**
     * @brief 基础移动速度（单位：cm/s）
     *
     * 默认值 300.0f 对应每秒移动 3 个标准格（每格 100cm）。
     * 实际速度可由外部系统（地形、状态效果等）在此基础上进行倍率缩放。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Movement",
        meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm/s"))
    float BaseSpeed = 300.0f;

    /**
     * @brief 对角线通行规则
     *
     * 决定寻路算法在展开对角线邻居节点时采用何种约束策略。
     * Strict 模式可避免角色"切角"穿越障碍物边缘，适合大多数策略类游戏。
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Movement")
    EGridDiagonalRule DiagonalRule = EGridDiagonalRule::Strict;

    /**
     * @brief Time in seconds before a character in Manual mode automatically reverts to AI.
     *
     * When a character is idle in Manual mode (no player-issued move command) for this
     * duration, it switches back to AI and resumes its last AI destination.
     * Set to 0 to disable the auto-revert.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Control",
        meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
    float ManualModeTimeout = 5.0f;

    /**
     * @brief Rotation interpolation speed (degrees per second).
     *
     * Controls how fast the character turns toward the movement direction.
     * Higher values = snappier turns, lower values = smoother arcs.
     * Set to 0 for instant rotation (legacy behaviour).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Movement",
        meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "deg/s"))
    float RotationSpeed = 720.0f;
};
