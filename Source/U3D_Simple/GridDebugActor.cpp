// Copyright Epic Games, Inc. All Rights Reserved.

#include "GridDebugActor.h"
#include "GridMapSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
// 构造函数
// ─────────────────────────────────────────────────────────────────────────────

AGridDebugActor::AGridDebugActor()
{
	// 需要 Tick 来每帧刷新屏幕统计文字
	PrimaryActorTick.bCanEverTick = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 生命周期
// ─────────────────────────────────────────────────────────────────────────────

void AGridDebugActor::BeginPlay()
{
	Super::BeginPlay();

	// 以本 Actor 为 Outer 创建扫描器，确保其生命周期与 Actor 绑定
	Scanner = NewObject<UTerrainScanner>(this);
	RebuildGrid();
}

void AGridDebugActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World) return;

	UGridMapSubsystem* Subsystem = World->GetSubsystem<UGridMapSubsystem>();
	if (!Subsystem) return;

	// 屏幕 Log 每帧刷新（Key 固定，不堆叠）
	UpdateScreenMessages(Subsystem);
}

// ─────────────────────────────────────────────────────────────────────────────
// 公开接口
// ─────────────────────────────────────────────────────────────────────────────

void AGridDebugActor::RebuildGrid()
{
	if (!Scanner) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 清除上一次绘制的所有持久调试线，防止旧数据残留叠加
	FlushPersistentDebugLines(World);

	// 将编辑器面板上的配置同步到扫描器，再执行扫描
	Scanner->SlopeThreshold      = SlopeThreshold;
	Scanner->TraceHeight         = TraceHeight;
	Scanner->ObstacleMarginCells = ObstacleMarginCells;
	Scanner->MaterialRules       = MaterialRules;
	Scanner->ScanTerrain(this);

	UGridMapSubsystem* Subsystem = World->GetSubsystem<UGridMapSubsystem>();
	if (!Subsystem) return;

	if (bDebugDraw)
	{
		DrawDebugGrid(World, Subsystem);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// 私有实现
// ─────────────────────────────────────────────────────────────────────────────

void AGridDebugActor::DrawDebugGrid(UWorld* World, UGridMapSubsystem* Subsystem)
{
	TArray<FGridCell> AllCells;
	Subsystem->GetAllCells(AllCells);

	// 格子可视化尺寸：留 4cm 间隙，避免相邻方块边线重叠糊成一片
	const float HalfCell = UGridMapSubsystem::CellSize * 0.5f - 2.0f;
	const FVector BoxExtent(HalfCell, HalfCell, 4.0f);

	// Lifetime = -1 表示持久显示，直到下次 FlushPersistentDebugLines 被调用
	const float Lifetime = -1.0f;

	for (const FGridCell& Cell : AllCells)
	{
		FColor Color;
		if (Cell.bWalkable)
		{
			// MoveCost gradient: 1.0=green, 1.5=yellow, >=2.0=red
			if (FMath::IsNearlyEqual(Cell.MoveCost, 1.f))
			{
				Color = FColor(0, 200, 0);       // green: default cost
			}
			else
			{
				float T = FMath::Clamp((Cell.MoveCost - 1.f) / 1.f, 0.f, 1.f);
				uint8 R = static_cast<uint8>(FMath::Lerp(200.f, 220.f, T));
				uint8 G = static_cast<uint8>(FMath::Lerp(200.f, 0.f, T));
				Color = FColor(R, G, 0);
			}
		}
		else if (Cell.BlockReason == EGridCellBlockReason::Material)
		{
			Color = FColor(139, 0, 0);       // dark red: material-blocked
		}
		else if (Cell.BlockReason == EGridCellBlockReason::Slope)
		{
			Color = FColor(255, 140, 0);     // orange: slope
		}
		else if (Cell.BlockReason == EGridCellBlockReason::NavMesh)
		{
			Color = FColor(200, 0, 0);       // red: NavMesh unreachable
		}
		else if (Cell.BlockReason == EGridCellBlockReason::ObstacleMargin)
		{
			Color = FColor(200, 100, 100);   // light red: obstacle margin
		}
		else
		{
			Color = FColor(80, 80, 80);      // dark grey: no surface
		}

		// 绘制位置抬高 5cm，避免与地面产生 z-fighting 闪烁
		FVector DrawCenter = Cell.WorldLocation + FVector(0.f, 0.f, 5.0f);
		DrawDebugBox(World, DrawCenter, BoxExtent, FQuat::Identity, Color,
		             true /*bPersistent*/, Lifetime, 0, 1.0f /*Thickness*/);

		// Material-blocked cells: draw an X across the cell
		if (!Cell.bWalkable && Cell.BlockReason == EGridCellBlockReason::Material)
		{
			FVector Corner1 = DrawCenter + FVector(-HalfCell, -HalfCell, 0.f);
			FVector Corner2 = DrawCenter + FVector( HalfCell,  HalfCell, 0.f);
			FVector Corner3 = DrawCenter + FVector( HalfCell, -HalfCell, 0.f);
			FVector Corner4 = DrawCenter + FVector(-HalfCell,  HalfCell, 0.f);
			DrawDebugLine(World, Corner1, Corner2, FColor(139, 0, 0),
			              true, Lifetime, 0, 1.5f);
			DrawDebugLine(World, Corner3, Corner4, FColor(139, 0, 0),
			              true, Lifetime, 0, 1.5f);
		}

		// Floating text for cells with non-default MoveCost or SpeedMultiplier
		if (Cell.bWalkable &&
			(!FMath::IsNearlyEqual(Cell.MoveCost, 1.f) ||
			 !FMath::IsNearlyEqual(Cell.SpeedMultiplier, 1.f)))
		{
			FString Label = FString::Printf(TEXT("C:%.1f S:%.1f"),
				Cell.MoveCost, Cell.SpeedMultiplier);
			DrawDebugString(World, DrawCenter + FVector(0.f, 0.f, 15.f),
				Label, nullptr, FColor::White, Lifetime, false, 0.8f);
		}
	}
}

void AGridDebugActor::UpdateScreenMessages(UGridMapSubsystem* Subsystem)
{
	if (!GEngine || !Scanner) return;

	// Line 1: grid scan stats (Key=100)
	FString Msg = FString::Printf(
	    TEXT("[Grid] Total:%d | Walk:%d | Blocked(Nav):%d | Blocked(Slope):%d | Blocked(Margin):%d | Scan:%.1fms"),
	    Scanner->GetLastTotalCount(),
	    Scanner->GetLastWalkableCount(),
	    Scanner->GetLastNavBlockedCount(),
	    Scanner->GetLastSlopeBlockedCount(),
	    Scanner->GetLastMarginBlockedCount(),
	    Scanner->GetLastScanTimeMs()
	);
	GEngine->AddOnScreenDebugMessage(100, 99999.f, FColor::Cyan, Msg);

	// Line 2: material rule stats (Key=106) — only shown when material rules are active
	if (Scanner->MaterialRules)
	{
		int32 DefaultCount = Scanner->GetLastTotalCount() - Scanner->GetLastMatMatchedCount();
		FString MatMsg = FString::Printf(
		    TEXT("[MatRule] Matched:%d | Walk%s:%d | Cost%s:%d | Speed%s:%d | Default:%d"),
		    Scanner->GetLastMatMatchedCount(),
		    TEXT("\u2193"), Scanner->GetLastMatWalkBlockedCount(),
		    TEXT("\u2191"), Scanner->GetLastMatCostModifiedCount(),
		    TEXT("\u2193"), Scanner->GetLastMatSpeedModifiedCount(),
		    DefaultCount
		);
		GEngine->AddOnScreenDebugMessage(106, 99999.f, FColor::Yellow, MatMsg);
	}
}
