// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainScanner.h"
#include "GridMapSubsystem.h"
#include "GridMaterialRuleDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// 直接引用子系统中定义的格子尺寸常量，避免魔法数字
static constexpr float CellSize = UGridMapSubsystem::CellSize;

void UTerrainScanner::ScanTerrain(UObject* WorldContextObject)
{
	// 步骤 1：记录开始时间，用于计算本次扫描耗时
	const double StartTime = FPlatformTime::Seconds();

	// 步骤 2：获取 UWorld，上下文对象无效时提前退出
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainScanner] 无法获取 UWorld，扫描中止"));
		return;
	}

	// 步骤 3：获取网格子系统，子系统不存在时提前退出
	UGridMapSubsystem* GridMapSubsystem = World->GetSubsystem<UGridMapSubsystem>();
	if (!GridMapSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainScanner] 无法获取 UGridMapSubsystem，扫描中止"));
		return;
	}

	// 步骤 4：清空旧的网格数据，确保本次扫描结果干净
	GridMapSubsystem->ClearAllCells();

	// 步骤 5：收集场景中所有带 "GridTerrain" Actor Tag 的 Actor
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsWithTag(World, FName("GridTerrain"), Actors);

	// 步骤 6：无可用地形 Actor 时报警并退出
	if (Actors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TerrainScanner] 场景中未找到带 'GridTerrain' Tag 的 Actor，扫描中止"));
		return;
	}

	// 步骤 7：将所有 Actor 的包围盒合并为统一 AABB
	// 使用第一个 Actor 的包围盒作为初始值，避免从零点开始 Union 导致范围虚增
	FBox CombinedBounds = Actors[0]->GetComponentsBoundingBox(true);
	for (int32 i = 1; i < Actors.Num(); ++i)
	{
		CombinedBounds += Actors[i]->GetComponentsBoundingBox(true);
	}

	// 步骤 8：根据 AABB 计算网格的起点坐标与格子数量
	// 向下对齐到格子边界，保证格子与世界坐标系对齐
	const float GridOriginX = FMath::FloorToFloat(CombinedBounds.Min.X / CellSize) * CellSize;
	const float GridOriginY = FMath::FloorToFloat(CombinedBounds.Min.Y / CellSize) * CellSize;

	// 上限 200 × 200 = 40000 格，防止异常大包围盒导致内存爆炸
	const int32 GridCountX = FMath::Min(FMath::CeilToInt((CombinedBounds.Max.X - GridOriginX) / CellSize), 200);
	const int32 GridCountY = FMath::Min(FMath::CeilToInt((CombinedBounds.Max.Y - GridOriginY) / CellSize), 200);

	// 将世界起点转换为格子坐标系的偏移量。
	// TerrainScanner 必须用绝对格坐标（而非局部 ix/iy 索引）存储格子，
	// 这样 WorldToGrid(worldPos) 返回的坐标才能与 GridCells 中的键一一对应。
	// 若用 ix/iy 直接存储，地形不在世界原点时两个坐标系完全错位，
	// 导致 GetCell / IsCellWalkable 拿到错误格子，角色移动到错误位置（乱飞）。
	const int32 OriginGridX = FMath::FloorToInt(GridOriginX / CellSize);
	const int32 OriginGridY = FMath::FloorToInt(GridOriginY / CellSize);

	// 步骤 9：获取导航系统，用于后续 NavMesh 可达性检查（允许为空，检查时跳过）
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	// 步骤 9.5：构建"仅命中地形"的射线查询参数
	// 原理：LineTrace 遇到第一个 blocking hit 即停止，障碍盒会遮挡地面。
	// 解决方案：将所有非 GridTerrain Actor 加入忽略列表，地面检测射线直接穿透障碍盒。
	// 障碍物阻挡依然由 NavMesh 检查判断，职责分离。
	FCollisionQueryParams TerrainQueryParams(NAME_None, false);
	TerrainQueryParams.bReturnPhysicalMaterial = true;
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
		for (AActor* Actor : AllActors)
		{
			if (!Actor->ActorHasTag(FName("GridTerrain")))
			{
				TerrainQueryParams.AddIgnoredActor(Actor);
			}
		}
	}

	// 步骤 10：重置本次扫描的统计计数
	LastTotalCount             = 0;
	LastWalkableCount          = 0;
	LastNavBlockedCount        = 0;
	LastSlopeBlockedCount      = 0;
	LastMarginBlockedCount     = 0;
	LastMatMatchedCount        = 0;
	LastMatWalkBlockedCount    = 0;
	LastMatCostModifiedCount   = 0;
	LastMatSpeedModifiedCount  = 0;

	// 步骤 11：双层循环遍历所有格子，执行射线检测与通行性判定
	for (int32 ix = 0; ix < GridCountX; ++ix)
	{
		for (int32 iy = 0; iy < GridCountY; ++iy)
		{
			// a. 计算格子中心的世界 XY 坐标及垂直射线的起止点
			// 射线从 AABB 顶部上方 TraceHeight 处向下发射，覆盖地表最高点
			const float CellCenterX = GridOriginX + ix * CellSize + CellSize * 0.5f;
			const float CellCenterY = GridOriginY + iy * CellSize + CellSize * 0.5f;

			const FVector TraceStart(CellCenterX, CellCenterY, CombinedBounds.Max.Z + TraceHeight);
			const FVector TraceEnd  (CellCenterX, CellCenterY, CombinedBounds.Min.Z - 100.0f);

			// b. 使用地形专用查询参数执行射线检测（忽略障碍物，仅命中 GridTerrain Actor）
			FHitResult HitResult;
			const bool bHit = World->LineTraceSingleByChannel(
				HitResult, TraceStart, TraceEnd, ECC_WorldStatic, TerrainQueryParams);

			// c. 未命中：该格子下方无静态地表，标记为 NoSurface 阻挡
			if (!bHit)
			{
				FGridCell Cell;
				Cell.GridCoord     = FIntPoint(OriginGridX + ix, OriginGridY + iy);
				Cell.WorldLocation = FVector(CellCenterX, CellCenterY, 0.f);
				Cell.bWalkable     = false;
				Cell.BlockReason   = EGridCellBlockReason::NoSurface;

				GridMapSubsystem->SetCell(FIntPoint(OriginGridX + ix, OriginGridY + iy), Cell);
				++LastTotalCount;
				continue;
			}

			// d. 计算地表坡度角：法线与世界 Up 向量夹角（单位：度）
			// 公式：θ = arccos(Normal · UpVector)，结果范围 [0°, 180°]
			const float SlopeAngle = FMath::RadiansToDegrees(
				FMath::Acos(FVector::DotProduct(HitResult.ImpactNormal, FVector::UpVector))
			);

			// e. 构造格子基础数据，WorldLocation.Z 使用地表命中点的真实高度
			FGridCell Cell;
			Cell.GridCoord     = FIntPoint(OriginGridX + ix, OriginGridY + iy);
			Cell.WorldLocation = FVector(CellCenterX, CellCenterY, HitResult.ImpactPoint.Z);
			Cell.bWalkable    = true;
			Cell.BlockReason  = EGridCellBlockReason::None;

			// f. 坡度检查：超过阈值则标记为坡度阻挡，跳过 NavMesh 检查
			if (SlopeAngle > SlopeThreshold)
			{
				Cell.bWalkable   = false;
				Cell.BlockReason = EGridCellBlockReason::Slope;
				++LastSlopeBlockedCount;
			}
			// g. NavMesh 检查：仅在坡度通过后执行，投影点不存在则标记为 NavMesh 阻挡
			else if (NavSys != nullptr)
			{
				FNavLocation NavLocation;
				// 查询半径在 XY 方向取格子半尺寸，Z 方向允许 50cm 浮动以容纳高度误差
				const FVector QueryExtent(CellSize * 0.5f, CellSize * 0.5f, 50.0f);

				if (!NavSys->ProjectPointToNavigation(Cell.WorldLocation, NavLocation, QueryExtent))
				{
					Cell.bWalkable   = false;
					Cell.BlockReason = EGridCellBlockReason::NavMesh;
					++LastNavBlockedCount;
				}
			}

			// h. Material rule matching via UE5 Physical Material system
			if (MaterialRules && Cell.bWalkable)
			{
				UPhysicalMaterial* HitPhysMat = HitResult.PhysMaterial.Get();

				if (HitPhysMat)
				{
					const bool bWasWalkable = Cell.bWalkable;

					if (MaterialRules->TryMatchMaterial(HitPhysMat, Cell))
					{
						++LastMatMatchedCount;

						if (!FMath::IsNearlyEqual(Cell.MoveCost, 1.f))
						{
							++LastMatCostModifiedCount;
						}
						if (!FMath::IsNearlyEqual(Cell.SpeedMultiplier, 1.f))
						{
							++LastMatSpeedModifiedCount;
						}
						if (bWasWalkable && !Cell.bWalkable)
						{
							Cell.BlockReason = EGridCellBlockReason::Material;
							++LastMatWalkBlockedCount;
						}
					}
				}
			}

			// i. 将格子写入子系统，并更新统计计数
			GridMapSubsystem->SetCell(FIntPoint(OriginGridX + ix, OriginGridY + iy), Cell);
			++LastTotalCount;
			if (Cell.bWalkable)
			{
				++LastWalkableCount;
			}
		}
	}

	// 步骤 12：障碍缓冲区后处理 —— 将靠近障碍的可通行格标记为不可通行
	// BFS 扩展：从所有已阻挡格向外扩展 ObstacleMarginCells 层
	if (ObstacleMarginCells > 0)
	{
		// 8-direction offsets for neighbour lookup
		static const FIntPoint Dirs[] = {
			{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
		};

		// Seed the BFS frontier with all currently-blocked cells
		TArray<FIntPoint> Frontier;
		{
			TArray<FGridCell> AllCells;
			GridMapSubsystem->GetAllCells(AllCells);
			for (const FGridCell& C : AllCells)
			{
				if (!C.bWalkable)
				{
					Frontier.Add(C.GridCoord);
				}
			}
		}

		// Expand layer by layer up to ObstacleMarginCells depth
		for (int32 Layer = 0; Layer < ObstacleMarginCells; ++Layer)
		{
			TArray<FIntPoint> NextFrontier;

			for (const FIntPoint& BlockedCoord : Frontier)
			{
				for (const FIntPoint& Dir : Dirs)
				{
					FIntPoint Neighbour = BlockedCoord + Dir;
					FGridCell NeighCell;

					if (GridMapSubsystem->GetCell(Neighbour, NeighCell) && NeighCell.bWalkable)
					{
						NeighCell.bWalkable   = false;
						NeighCell.BlockReason = EGridCellBlockReason::ObstacleMargin;
						GridMapSubsystem->SetCell(Neighbour, NeighCell);

						++LastMarginBlockedCount;
						--LastWalkableCount;

						NextFrontier.Add(Neighbour);
					}
				}
			}

			Frontier = MoveTemp(NextFrontier);
		}
	}

	// 步骤 13：计算并记录本次扫描总耗时（秒 → 毫秒）
	LastScanTimeMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

	// 步骤 14：输出扫描结果摘要日志，便于调试与性能分析
	UE_LOG(LogTemp, Log,
		TEXT("[TerrainScanner] Scan done: Total=%d Walk=%d NavBlocked=%d SlopeBlocked=%d MarginBlocked=%d MatMatched=%d MatWalkBlocked=%d Time=%.1fms"),
		LastTotalCount, LastWalkableCount, LastNavBlockedCount, LastSlopeBlockedCount,
		LastMarginBlockedCount, LastMatMatchedCount, LastMatWalkBlockedCount, LastScanTimeMs
	);
}
