// Copyright Epic Games, Inc. All Rights Reserved.

#include "GridMapSubsystem.h"
#include "TerrainScanner.h"
#include "NavigationSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
// 生命周期与扫描器管理
// ─────────────────────────────────────────────────────────────────────────────

void UGridMapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 监听 NavMesh 生成完成事件
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld);
	if (NavSys)
	{
		NavSys->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &UGridMapSubsystem::OnNavMeshGenerated);
	}
}

void UGridMapSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (World)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys)
		{
			NavSys->OnNavigationGenerationFinishedDelegate.RemoveAll(this);
		}
	}

	Super::Deinitialize();
}

void UGridMapSubsystem::SetActiveScanner(UTerrainScanner* InScanner)
{
	ActiveScanner = InScanner;
}

void UGridMapSubsystem::OnNavMeshGenerated(ANavigationData* NavData)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(110, 5.f, FColor::Cyan, TEXT("[Nav] NavMesh ready -> triggering scan"));
	}

	if (ActiveScanner)
	{
		int32 OldWalkableCount = GetWalkableCellCount();
		
		// 重新扫描
		ActiveScanner->ScanTerrain(this);
		
		int32 NewWalkableCount = GetWalkableCellCount();
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(111, 5.f, FColor::Cyan, 
				FString::Printf(TEXT("[Grid] Rescan done | Walk:%d (was %d, %d blocked by obstacles)"), 
					NewWalkableCount, OldWalkableCount, OldWalkableCount - NewWalkableCount));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 数据写入
// ─────────────────────────────────────────────────────────────────────────────

void UGridMapSubsystem::SetCell(const FIntPoint& Coord, const FGridCell& Cell)
{
	// 直接覆盖写入；TMap::Add 在键已存在时覆盖值，键不存在时插入新条目
	GridCells.Add(Coord, Cell);
}

void UGridMapSubsystem::SetCellWalkable(const FIntPoint& Coord, bool bWalkable)
{
	// 仅修改已有格子的通行性，不主动创建新格子，避免产生数据不完整的孤立条目
	FGridCell* cell = GridCells.Find(Coord);
	if (!cell)
	{
		return;
	}

	cell->bWalkable = bWalkable;
}

void UGridMapSubsystem::ClearAllCells()
{
	// 释放所有格子内存，重置到初始空状态
	GridCells.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// 数据查询
// ─────────────────────────────────────────────────────────────────────────────

bool UGridMapSubsystem::GetCell(const FIntPoint& Coord, FGridCell& OutCell) const
{
	const FGridCell* cell = GridCells.Find(Coord);
	if (!cell)
	{
		return false;
	}

	OutCell = *cell;
	return true;
}

bool UGridMapSubsystem::IsCellWalkable(const FIntPoint& Coord) const
{
	const FGridCell* cell = GridCells.Find(Coord);
	// 格子不存在视为不可通行，与寻路算法"未知格子=障碍"的惯例一致
	if (!cell)
	{
		return false;
	}

	return cell->bWalkable;
}

int32 UGridMapSubsystem::GetTotalCellCount() const
{
	return GridCells.Num();
}

int32 UGridMapSubsystem::GetWalkableCellCount() const
{
	int32 count = 0;
	for (const TPair<FIntPoint, FGridCell>& pair : GridCells)
	{
		if (pair.Value.bWalkable)
		{
			++count;
		}
	}
	return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// 坐标互转
// ─────────────────────────────────────────────────────────────────────────────

FIntPoint UGridMapSubsystem::WorldToGrid(const FVector& WorldLocation) const
{
	// 公式：Coord = Floor(WorldLocation.XY / CellSize)
	// 使用 FloorToInt 而非强制转型，保证负坐标（如 X=-50）能正确落到格子 -1 而非 0
	const int32 col = FMath::FloorToInt(WorldLocation.X / CellSize);
	const int32 row = FMath::FloorToInt(WorldLocation.Y / CellSize);
	return FIntPoint(col, row);
}

FVector UGridMapSubsystem::GridToWorld(const FIntPoint& Coord) const
{
	// 平面中心点：格坐标 * 格尺寸 + 半格偏移，使返回点落在格子正中央
	const float centerX = Coord.X * CellSize + CellSize * 0.5f;
	const float centerY = Coord.Y * CellSize + CellSize * 0.5f;

	// Z 轴取该格存储的地表高度采样值；格子不存在时回退到 0，
	// 调用方若需精确 Z 应先通过 GetCell() 确认格子存在
	float centerZ = 0.0f;
	const FGridCell* cell = GridCells.Find(Coord);
	if (cell)
	{
		centerZ = cell->WorldLocation.Z;
	}

	return FVector(centerX, centerY, centerZ);
}

// ─────────────────────────────────────────────────────────────────────────────
// AI 随机采样
// ─────────────────────────────────────────────────────────────────────────────

bool UGridMapSubsystem::GetRandomWalkableCell(FIntPoint& OutCoord) const
{
	// 先收集所有可通行格的坐标，再用随机索引选取。
	// 虽然每次调用都会构建临时数组，但 AI 采样频率通常极低（秒级），
	// 此处简洁性优先于极致性能。
	TArray<FIntPoint> walkableCoords;
	walkableCoords.Reserve(GridCells.Num());

	for (const TPair<FIntPoint, FGridCell>& pair : GridCells)
	{
		if (pair.Value.bWalkable)
		{
			walkableCoords.Add(pair.Key);
		}
	}

	if (walkableCoords.IsEmpty())
	{
		return false;
	}

	// FMath::RandRange 含两端，索引范围 [0, Num-1]
	const int32 randomIndex = FMath::RandRange(0, walkableCoords.Num() - 1);
	OutCoord = walkableCoords[randomIndex];
	return true;
}

bool UGridMapSubsystem::GetRandomWalkableCellInRadius(FIntPoint Center, float WorldRadius, FIntPoint& OutCoord) const
{
	// 半径无效时退化为全图随机采样
	if (WorldRadius <= 0.f)
	{
		return GetRandomWalkableCell(OutCoord);
	}

	// 将世界半径换算为格子数（向上取整，保证边界格也被纳入候选）
	const float RadiusInCells = WorldRadius / CellSize;

	TArray<FIntPoint> Candidates;
	Candidates.Reserve(FMath::Min(GridCells.Num(), 256));

	for (const TPair<FIntPoint, FGridCell>& Pair : GridCells)
	{
		if (!Pair.Value.bWalkable)
		{
			continue;
		}

		// 用欧氏距离判断格子中心是否在圆形范围内
		const float dx = static_cast<float>(Pair.Key.X - Center.X);
		const float dy = static_cast<float>(Pair.Key.Y - Center.Y);
		if (FMath::Sqrt(dx * dx + dy * dy) <= RadiusInCells)
		{
			Candidates.Add(Pair.Key);
		}
	}

	if (!Candidates.IsEmpty())
	{
		OutCoord = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		return true;
	}

	// 半径内无可通行格时，降级到全图采样，保证 AI 不会因半径设置过小而永远卡住
	return GetRandomWalkableCell(OutCoord);
}

void UGridMapSubsystem::GetAllCells(TArray<FGridCell>& OutCells) const
{
	GridCells.GenerateValueArray(OutCells);
}
