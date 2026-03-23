// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GridTypes.generated.h"

/**
 * @enum EGridCellBlockReason
 * @brief 格子不可通行的具体原因 - 用于调试与可视化展示
 * @note 仅在 bWalkable == false 时有意义；可通行格子的值固定为 None
 */
UENUM(BlueprintType)
enum class EGridCellBlockReason : uint8
{
    /** 无阻挡，格子正常可通行 */
    None        UMETA(DisplayName = "None"),

    /** 地表坡度超过 SlopeThreshold，角色无法站立 */
    Slope       UMETA(DisplayName = "Slope Too Steep"),

    /** NavMesh 未覆盖该位置，寻路系统认为不可达 */
    NavMesh     UMETA(DisplayName = "NavMesh Unreachable"),

    /** LineTrace 未命中任何地表，该格子悬空或超出场景范围 */
    NoSurface       UMETA(DisplayName = "No Surface Found"),

    /** 格子本身可通行，但处于障碍物边缘缓冲区内，被 ObstacleMarginCells 规则标记 */
    ObstacleMargin  UMETA(DisplayName = "Obstacle Margin"),

    /** 材质规则将该格强制标记为不可通行 */
    Material        UMETA(DisplayName = "Material Rule")
};

/**
 * @struct FGridCell
 * @brief 网格单元格数据结构 - 描述网格移动系统中单个格子的逻辑与物理属性
 * @note 用于网格寻路、移动代价计算及地表材质标记
 */
USTRUCT(BlueprintType)
struct U3D_SIMPLE_API FGridCell
{
	GENERATED_BODY()

	/** 格子的逻辑坐标（列, 行） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FIntPoint GridCoord;

	/** 格子的世界坐标，Z轴包含地表高度采样结果 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector WorldLocation;

	/** 是否可行走；false 表示障碍物，寻路时跳过该格 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	bool bWalkable;

	/** 进入该格子所需的基础移动代价，用于 A* 估价函数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float MoveCost;

	/** 在该格子上移动时的速度倍率（1.0 = 正常速度，< 1.0 = 减速地形） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float SpeedMultiplier;

	/** 地表材质标签，用于驱动音效、粒子、动画等表现层逻辑 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FGameplayTag MaterialTag;

	/** 格子不可通行的具体原因；bWalkable == true 时该值恒为 None */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	EGridCellBlockReason BlockReason;

	/** 默认构造函数，显式初始化所有字段为安全默认值 */
	FGridCell()
		: GridCoord(FIntPoint::ZeroValue)
		, WorldLocation(FVector::ZeroVector)
		, bWalkable(true)
		, MoveCost(1.0f)
		, SpeedMultiplier(1.0f)
		, MaterialTag()
		, BlockReason(EGridCellBlockReason::None)
	{
	}
};
