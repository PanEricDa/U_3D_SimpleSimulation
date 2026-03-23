// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GridTypes.h"
#include "GridMapSubsystem.generated.h"

/**
 * @class UGridMapSubsystem
 * @brief 网格地图子系统 - 全局单例，随 World 生命周期自动创建与销毁
 *
 * 负责持有整个关卡的网格数据，提供格子的增删查改、坐标互转以及 AI 随机采样接口。
 * 继承自 UWorldSubsystem，无需手动实例化，通过 GetWorld()->GetSubsystem<UGridMapSubsystem>() 获取。
 *
 * @note 格子尺寸固定为 CellSize = 100.0f（1m × 1m），不对外开放配置，
 *       保证寻路算法对格子边长的假设始终成立。
 */
UCLASS()
class U3D_SIMPLE_API UGridMapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------
	// 格子尺寸常量
	// -------------------------------------------------------------------------

	/**
	 * 每个格子的世界空间边长（单位：cm）。
	 * 固定为 100.0f（即 1m × 1m），设计上不允许在运行时修改，
	 * 以确保 WorldToGrid / GridToWorld 互转的一致性。
	 */
	static constexpr float CellSize = 100.0f;

	// -------------------------------------------------------------------------
	// 数据写入接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 写入或覆盖一个格子
	 * @param Coord  目标格子的逻辑坐标（列, 行）
	 * @param Cell   要写入的格子数据（完整覆盖）
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Write")
	void SetCell(const FIntPoint& Coord, const FGridCell& Cell);

	/**
	 * @brief 运行时动态覆写指定格子的可通行性
	 * @param Coord     目标格子的逻辑坐标
	 * @param bWalkable 新的可通行状态（true = 可通行，false = 障碍）
	 * @note  若格子不存在则静默忽略，不会创建新格子
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Write")
	void SetCellWalkable(const FIntPoint& Coord, bool bWalkable);

	/**
	 * @brief 清空全部网格数据
	 * @note  通常在关卡重新生成网格前调用，避免脏数据残留
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Write")
	void ClearAllCells();

	// -------------------------------------------------------------------------
	// 数据查询接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 查询指定坐标的格子数据
	 * @param Coord    目标格子的逻辑坐标
	 * @param OutCell  [out] 查询成功时填充格子数据
	 * @return 格子存在返回 true，否则返回 false，OutCell 保持不变
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Query")
	bool GetCell(const FIntPoint& Coord, FGridCell& OutCell) const;

	/**
	 * @brief 快速查询指定格子是否可通行
	 * @param Coord 目标格子的逻辑坐标
	 * @return 格子存在且 bWalkable == true 时返回 true，格子不存在也返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Query")
	bool IsCellWalkable(const FIntPoint& Coord) const;

	/**
	 * @brief 返回当前持有的格子总数
	 * @return 格子总数（包含不可通行格子）
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Query")
	int32 GetTotalCellCount() const;

	/**
	 * @brief 返回当前可通行格子的数量
	 * @return bWalkable == true 的格子数
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Query")
	int32 GetWalkableCellCount() const;

	// -------------------------------------------------------------------------
	// 坐标互转接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 世界坐标 → 逻辑格坐标
	 *
	 * 转换公式：Coord = Floor(WorldLocation / CellSize)
	 * 向下取整确保负坐标也能正确映射到格子象限。
	 *
	 * @param WorldLocation 世界空间坐标（Z 轴不参与平面坐标计算）
	 * @return 对应的逻辑格坐标（列, 行）
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
	FIntPoint WorldToGrid(const FVector& WorldLocation) const;

	/**
	 * @brief 逻辑格坐标 → 世界坐标格子中心点
	 *
	 * 平面转换公式：XY = Coord * CellSize + CellSize * 0.5
	 * Z 轴从 GridCells 中查找该格存储的 WorldLocation.Z；
	 * 若格子不存在则 Z = 0，调用方需注意此边界情况。
	 *
	 * @param Coord 逻辑格坐标（列, 行）
	 * @return 格子中心点的世界坐标
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
	FVector GridToWorld(const FIntPoint& Coord) const;

	// -------------------------------------------------------------------------
	// AI 随机采样接口
	// -------------------------------------------------------------------------

	/**
	 * @brief 从所有可通行格中随机返回一个格子坐标（供 AI 行为树使用）
	 * @param OutCoord [out] 随机选中的可通行格坐标
	 * @return 存在可通行格时返回 true；格子表为空或全为障碍时返回 false
	 * @note 使用 FMath::RandRange 进行随机索引选取，非加权均匀分布
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|AI")
	bool GetRandomWalkableCell(FIntPoint& OutCoord) const;

	/**
	 * @brief 在指定中心格的世界半径范围内随机返回一个可通行格坐标
	 * @param Center      搜索中心的逻辑格坐标
	 * @param WorldRadius 搜索半径（世界单位 cm）；传入 0 或负值时等同于 GetRandomWalkableCell（全图）
	 * @param OutCoord    [out] 随机选中的可通行格坐标
	 * @return 半径内存在可通行格时返回 true，否则降级到全图采样；全图也无格时返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid|AI")
	bool GetRandomWalkableCellInRadius(FIntPoint Center, float WorldRadius, FIntPoint& OutCoord) const;

	/** 获取所有格子数据（供调试可视化遍历用） */
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void GetAllCells(TArray<FGridCell>& OutCells) const;

private:

	// -------------------------------------------------------------------------
	// 内部数据
	// -------------------------------------------------------------------------

	/**
	 * 网格数据主存储。
	 *
	 * 注意：TMap<FIntPoint, FGridCell> 无法直接使用 UPROPERTY 宏，
	 * 原因是 UE 反射系统要求 TMap 的键类型必须实现 GetValueTypeHash() 且被
	 * UHT（Unreal Header Tool）识别为合法键类型。FIntPoint 虽然有哈希支持，
	 * 但 UHT 目前不将其列为 TMap UPROPERTY 的合法键，编译时会产生
	 * "Map properties with non-UObject values require a native getter" 类错误。
	 * 因此改为普通 C++ 成员，由 C++ 代码直接管理生命周期，蓝图通过上方的
	 * BlueprintCallable 函数间接访问数据。
	 */
	TMap<FIntPoint, FGridCell> GridCells;
};
