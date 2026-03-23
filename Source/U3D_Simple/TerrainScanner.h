// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GridTypes.h"
#include "GridMaterialRuleDataAsset.h"
#include "TerrainScanner.generated.h"

/**
 * @class UTerrainScanner
 * @brief 地形扫描器 - 通过 LineTrace 与 NavMesh 查询自动填充 UGridMapSubsystem 的网格数据
 *
 * 工作流程：
 *   1. 收集场景中所有带 "GridTerrain" Tag 的 Actor，计算合并 AABB
 *   2. 按 UGridMapSubsystem::CellSize 划分网格，对每格垂直投射射线采样地表高度
 *   3. 依次进行坡度检查（SlopeThreshold）与 NavMesh 可达性检查
 *   4. 将结果写入 UGridMapSubsystem，并记录本次扫描的统计数据
 *
 * @note 使用 WorldContext 模式获取 UWorld，可在蓝图或 C++ 中任意时机调用
 */
UCLASS(Blueprintable, BlueprintType)
class U3D_SIMPLE_API UTerrainScanner : public UObject
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------
	// 配置属性
	// -------------------------------------------------------------------------

	/**
	 * 最大可通行坡度（度）。
	 * 地表法线与世界 Up 向量的夹角超过该值时，格子被标记为 Slope 阻挡。
	 * 典型值：45.0f（更陡的坡不可行走）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	float SlopeThreshold = 45.0f;

	/**
	 * LineTrace 起点高于 AABB 上边界的额外偏移距离（cm）。
	 * 确保射线从场景几何体上方发射，避免起点陷入地表内部导致 miss。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	float TraceHeight = 200.0f;

	/**
	 * Number of cell layers to mark as impassable around every obstacle.
	 * 0 = disabled (no margin).
	 * 1 = any walkable cell touching an obstacle becomes impassable.
	 * 2 = two layers of buffer, etc.
	 * Applied as a post-scan pass after all other checks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 ObstacleMarginCells = 1;

	/**
	 * Material rule asset for overriding cell properties based on surface material.
	 * Leave null to skip material matching entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	TObjectPtr<UGridMaterialRuleDataAsset> MaterialRules;

	// -------------------------------------------------------------------------
	// 公开接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 主扫描函数 - 对场景进行完整的地形采样并更新 UGridMapSubsystem
	 * @param WorldContextObject 提供 UWorld 上下文的对象（通常传入 self 或关卡 Actor）
	 * @note  扫描期间会先调用 ClearAllCells()，旧数据将全部清除
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void ScanTerrain(UObject* WorldContextObject);

	/**
	 * @brief 获取上次扫描处理的格子总数（包含不可通行格子）
	 * @return 上次扫描总格数；未扫描过时返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastTotalCount() const { return LastTotalCount; }

	/**
	 * @brief 获取上次扫描中被判定为可通行的格子数量
	 * @return 上次扫描可通行格数；未扫描过时返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastWalkableCount() const { return LastWalkableCount; }

	/**
	 * @brief 获取上次扫描中因 NavMesh 不可达而被阻挡的格子数量
	 * @return 上次扫描 NavMesh 阻挡格数；未扫描过时返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastNavBlockedCount() const { return LastNavBlockedCount; }

	/**
	 * @brief 获取上次扫描中因坡度超限而被阻挡的格子数量
	 * @return 上次扫描坡度阻挡格数；未扫描过时返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastSlopeBlockedCount() const { return LastSlopeBlockedCount; }

	/**
	 * @brief 获取上次扫描的总耗时
	 * @return 扫描耗时（毫秒）；未扫描过时返回 0.0f
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastMarginBlockedCount() const { return LastMarginBlockedCount; }

	UFUNCTION(BlueprintCallable, Category = "Grid")
	float GetLastScanTimeMs() const { return LastScanTimeMs; }

	/** Number of cells that matched a material rule in the last scan. */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastMatMatchedCount() const { return LastMatMatchedCount; }

	/** Number of cells forced unwalkable by material rules in the last scan. */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastMatWalkBlockedCount() const { return LastMatWalkBlockedCount; }

	/** Number of cells with MoveCost modified by material rules (!= 1.0) in the last scan. */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastMatCostModifiedCount() const { return LastMatCostModifiedCount; }

	/** Number of cells with SpeedMultiplier modified by material rules (!= 1.0) in the last scan. */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetLastMatSpeedModifiedCount() const { return LastMatSpeedModifiedCount; }

private:

	// -------------------------------------------------------------------------
	// 扫描统计缓存（每次 ScanTerrain 前重置）
	// -------------------------------------------------------------------------

	/** 上次扫描处理的格子总数 */
	int32 LastTotalCount = 0;

	/** 上次扫描中判定为可通行的格子数 */
	int32 LastWalkableCount = 0;

	/** 上次扫描中因 NavMesh 不可达被阻挡的格子数 */
	int32 LastNavBlockedCount = 0;

	/** 上次扫描中因坡度超限被阻挡的格子数 */
	int32 LastSlopeBlockedCount = 0;

	/** 上次扫描中因障碍缓冲区被标记为不可通行的格子数 */
	int32 LastMarginBlockedCount = 0;

	/** 上次扫描总耗时（毫秒） */
	float LastScanTimeMs = 0.0f;

	/** Number of cells matched by material rules */
	int32 LastMatMatchedCount = 0;

	/** Number of cells forced unwalkable by material rules */
	int32 LastMatWalkBlockedCount = 0;

	/** Number of cells with MoveCost != 1.0 from material rules */
	int32 LastMatCostModifiedCount = 0;

	/** Number of cells with SpeedMultiplier != 1.0 from material rules */
	int32 LastMatSpeedModifiedCount = 0;
};
