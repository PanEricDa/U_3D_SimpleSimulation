// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TerrainScanner.h"
#include "GridMaterialRuleDataAsset.h"
#include "GridDebugActor.generated.h"

// 前向声明，避免循环包含
class UGridMapSubsystem;

/**
 * @class AGridDebugActor
 * @brief 网格调试 Actor - 负责驱动地形扫描并将网格数据以彩色方块可视化呈现在场景中
 *
 * 工作流程：
 *   1. BeginPlay 时创建 UTerrainScanner 并触发首次扫描
 *   2. RebuildGrid 清空旧的持久调试线后重新扫描，并绘制每个格子的彩色线框方块
 *   3. Tick 每帧刷新屏幕左上角的扫描统计文字（使用固定 Key，不产生堆叠）
 *
 * 颜色编码：
 *   绿色  = 可通行
 *   橙色  = 坡度阻挡
 *   红色  = NavMesh 不可达
 *   深灰  = 无地表命中
 *
 * @note 本 Actor 纯粹用于编辑器/开发期调试，正式发布前应从关卡中移除或禁用 bDebugDraw
 */
UCLASS()
class U3D_SIMPLE_API AGridDebugActor : public AActor
{
	GENERATED_BODY()

public:

	AGridDebugActor();

	// -------------------------------------------------------------------------
	// 配置属性
	// -------------------------------------------------------------------------

	/**
	 * 最大可通行坡度（度）。
	 * 超过该角度的地表格子将被标记为坡度阻挡（橙色）。
	 * 与 UTerrainScanner::SlopeThreshold 同步使用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	float SlopeThreshold = 45.0f;

	/**
	 * LineTrace 起点在 AABB 上边界之上的额外偏移距离（cm）。
	 * 与 UTerrainScanner::TraceHeight 同步使用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	float TraceHeight = 200.0f;

	/**
	 * Number of cell layers to mark as impassable around every obstacle.
	 * 0 = disabled. 1 = cells touching obstacles become impassable. etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 ObstacleMarginCells = 1;

	/**
	 * Material rule asset to apply during terrain scanning.
	 * Assign a UGridMaterialRuleDataAsset to enable per-material cell overrides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Scanner")
	TObjectPtr<UGridMaterialRuleDataAsset> MaterialRules;

	/**
	 * 是否在场景中绘制调试格子方块。
	 * false 时只扫描不绘制，适用于纯逻辑验证场景。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Debug")
	bool bDebugDraw = true;

	// -------------------------------------------------------------------------
	// 内部成员
	// -------------------------------------------------------------------------

	/** 地形扫描器实例，BeginPlay 时由 NewObject 创建 */
	UPROPERTY()
	UTerrainScanner* Scanner = nullptr;

	// -------------------------------------------------------------------------
	// 生命周期
	// -------------------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------------------
	// 公开接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 重新扫描地形并刷新调试可视化
	 * @note  会先清空所有持久调试线，再重新绘制；可在运行时从蓝图调用以热重建网格
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void RebuildGrid();

private:

	/**
	 * @brief 遍历 Subsystem 中所有格子并绘制对应颜色的线框方块
	 * @param World     当前 UWorld，用于 DrawDebugBox 调用
	 * @param Subsystem 持有格子数据的子系统引用
	 */
	void DrawDebugGrid(UWorld* World, UGridMapSubsystem* Subsystem);

	/**
	 * @brief 在屏幕左上角刷新本次扫描统计信息
	 * @param Subsystem 持有格子数据的子系统引用（用于未来扩展，当前统计来自 Scanner）
	 * @note  使用固定 Key=100 写入，每帧覆盖上一帧，不产生文字堆叠
	 */
	void UpdateScreenMessages(UGridMapSubsystem* Subsystem);
};
