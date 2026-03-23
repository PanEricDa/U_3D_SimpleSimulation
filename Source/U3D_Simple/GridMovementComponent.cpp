// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.

#include "GridMovementComponent.h"
#include "GridMapSubsystem.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// 构造函数
// ─────────────────────────────────────────────────────────────────────────────

UGridMovementComponent::UGridMovementComponent()
{
    // 必须显式启用，否则 TickComponent 永远不会被引擎调用，
    // 导致插值位移逻辑（StepAlpha 推进、到格判定、FinishMovement）无法执行
    PrimaryComponentTick.bCanEverTick = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
    {
        CachedCMC = Char->GetCharacterMovement();
        if (CachedCMC.IsValid())
        {
            // 切换为 MOVE_None 并关闭 CMC 的 Tick，让 CMC 完全退出运行。
            // MOVE_Custom 仍会每帧执行 CMC 内部状态机（地面检测、速度修正等），
            // 只有 MOVE_None + 禁用 Tick 才能彻底断开物理干扰。
            // 所有位移由本组件通过 SetActorLocation 独占驱动。
            CachedCMC->SetMovementMode(MOVE_None);
            CachedCMC->SetComponentTickEnabled(false);
            CachedCMC->GravityScale = 0.f;
            CachedCMC->bOrientRotationToMovement    = false;
            CachedCMC->bUseControllerDesiredRotation = false;
        }

        if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
        {
            CachedZOffset = Capsule->GetScaledCapsuleHalfHeight();
            // 关闭胶囊体物理模拟与重力，确保物理引擎不会推动胶囊体
            Capsule->SetSimulatePhysics(false);
            Capsule->SetEnableGravity(false);
        }
    }

    // 初始化当前格坐标（以 Actor 初始位置为基准）
    if (UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>())
    {
        CurrentCell = Sub->WorldToGrid(GetOwner()->GetActorLocation());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 非 Moving 状态仅刷新 Debug 可视化后退出
    if (MoveState != EGridMoveState::Moving)
    {
        DrawDebugInfo();
        return;
    }

    // ── 插值推进 ──────────────────────────────────────────────────────────────
    // 公式：Alpha += DeltaTime / StepDuration，线性推进当前步进度
    StepAlpha += DeltaTime / FMath::Max(StepDuration, KINDA_SMALL_NUMBER);
    float ClampedAlpha = FMath::Clamp(StepAlpha, 0.f, 1.f);

    FVector NewPos = FMath::Lerp(StepStartPos, StepTargetPos, ClampedAlpha);
    GetOwner()->SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);

    // Smooth rotation toward movement direction (XY only, ignore Z to prevent pitch tilt)
    FVector DirXY(StepTargetPos.X - StepStartPos.X, StepTargetPos.Y - StepStartPos.Y, 0.f);
    if (!DirXY.IsNearlyZero())
    {
        FRotator TargetRot = DirXY.ToOrientationRotator();
        float RotSpeed = (Config && Config->RotationSpeed > 0.f) ? Config->RotationSpeed : 0.f;
        if (RotSpeed > 0.f)
        {
            FRotator CurrentRot = GetOwner()->GetActorRotation();
            FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, RotSpeed);
            GetOwner()->SetActorRotation(NewRot);
        }
        else
        {
            // RotationSpeed == 0: instant snap (legacy behaviour)
            GetOwner()->SetActorRotation(TargetRot);
        }
    }

    // ── 到达本步目标格 ────────────────────────────────────────────────────────
    if (StepAlpha >= 1.0f)
    {
        // 更新当前格坐标并广播 OnCellEntered
        CurrentCell = GridPath[PathIndex];

        UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
        FGridCell CellData;
        if (Sub)
        {
            Sub->GetCell(CurrentCell, CellData);
        }
        OnCellEntered.Broadcast(CurrentCell, CellData);

        PathIndex++;
        if (PathIndex >= GridPath.Num())
        {
            // 所有步骤执行完毕，抵达终点
            FinishMovement();
        }
        else
        {
            // 继续执行下一步
            StartNextStep();
        }
    }

    DrawDebugInfo();
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveToWorldLocation
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::MoveToWorldLocation(FVector WorldPos)
{
    UWorld* World = GetWorld();
    if (!World) return;

    UGridMapSubsystem* Sub = World->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return;

    FIntPoint GoalCell = Sub->WorldToGrid(WorldPos);

    // 目标格必须可通行，否则直接拒绝请求
    if (!Sub->IsCellWalkable(GoalCell))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(101, 3.f, FColor::Red,
                TEXT("[Move] Target not walkable"));
        }
        return;
    }

    MoveToCell(GoalCell);
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveToCell
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::MoveToCell(FIntPoint TargetCellParam)
{
    UWorld* World = GetWorld();
    if (!World) return;

    UGridMapSubsystem* Sub = World->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return;

    // 相同格子无需移动，提前退出
    if (TargetCellParam == CurrentCell) return;

    SetMoveState(EGridMoveState::Pathfinding);

    // 构造寻路起点和终点的世界坐标
    FVector FromWorld = Sub->GridToWorld(CurrentCell);

    FGridCell TargetCellData;
    FVector ToWorld = Sub->GetCell(TargetCellParam, TargetCellData)
                      ? TargetCellData.WorldLocation
                      : Sub->GridToWorld(TargetCellParam);

    // 通过 NavMesh 生成路径并映射为格坐标序列
    TArray<FIntPoint> NewPath;
    if (!BuildGridPath(FromWorld, ToWorld, NewPath) || NewPath.IsEmpty())
    {
        // 寻路失败：切换 Blocked 状态并通知上层
        SetMoveState(EGridMoveState::Blocked);
        OnPathBlocked.Broadcast(TargetCellParam);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(101, 3.f, FColor::Orange,
                FString::Printf(TEXT("[Move] No path to (%d,%d)"),
                    TargetCellParam.X, TargetCellParam.Y));
        }
        return;
    }

    TargetCell = TargetCellParam;
    GridPath   = NewPath;
    PathIndex  = 0;

    // 启动第一步插值，再切 Moving（保证 StartNextStep 里读到正确的 GridPath）
    StartNextStep();
    SetMoveState(EGridMoveState::Moving);
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildGridPath
// ─────────────────────────────────────────────────────────────────────────────

bool UGridMovementComponent::BuildGridPath(const FVector& FromWorld, const FVector& ToWorld,
                                           TArray<FIntPoint>& OutPath)
{
    UWorld* World = GetWorld();
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    UGridMapSubsystem*   Sub    = World ? World->GetSubsystem<UGridMapSubsystem>() : nullptr;

    if (!NavSys || !Sub) return false;

    // 获取默认 NavData（NavMesh），不自动创建
    ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
    if (!NavData) return false;

    // 同步寻路：在当前帧内立即拿到路径结果
    UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(
        World, FromWorld, ToWorld);

    if (!NavPath || !NavPath->IsValid() || NavPath->PathPoints.IsEmpty())
    {
        return false;
    }

    // NavMesh 返回的是稀疏途经点（waypoints），相邻两点可能跨越几十格。
    // 用 Bresenham 直线算法在相邻途经点之间逐格填充，确保每步只移动 1 个格子（含对角），
    // 避免因跨格瞬移而导致角色"飞行"。
    OutPath.Empty();

    FIntPoint PrevCell = Sub->WorldToGrid(FromWorld);

    for (int32 i = 1; i < NavPath->PathPoints.Num(); ++i)
    {
        FIntPoint NextCell = Sub->WorldToGrid(NavPath->PathPoints[i]);

        // ── Bresenham 8 方向直线填充 ──────────────────────────────────────────
        // 若中途遇到不可通行格则停止本段填充，直接跳到 NavPath 途经点本身，
        // 确保路径中不含不可通行格，避免 StartNextStep 触发 Blocked 递归崩溃。
        int32 x0 = PrevCell.X, y0 = PrevCell.Y;
        int32 x1 = NextCell.X, y1 = NextCell.Y;
        int32 dx = FMath::Abs(x1 - x0);
        int32 dy = FMath::Abs(y1 - y0);
        int32 sx = (x1 > x0) ? 1 : -1;
        int32 sy = (y1 > y0) ? 1 : -1;
        int32 err = dx - dy;

        bool bSegmentClear = true;
        // Walk from PrevCell toward NextCell one grid step at a time.
        // Diagonal steps are validated against DiagonalRule to prevent corner-cutting.
        while (x0 != x1 || y0 != y1)
        {
            int32 e2 = 2 * err;
            bool bStepX = (e2 > -dy);
            bool bStepY = (e2 <  dx);

            FIntPoint StepFrom(x0, y0);

            if (bStepX && bStepY)
            {
                // Diagonal step — check if allowed by DiagonalRule
                FIntPoint DiagTarget(x0 + sx, y0 + sy);

                if (Sub->IsCellWalkable(DiagTarget) &&
                    IsDiagonalAllowed(StepFrom, DiagTarget, Sub))
                {
                    // Diagonal is safe — take it
                    err -= dy; x0 += sx;
                    err += dx; y0 += sy;
                }
                else
                {
                    // Diagonal blocked — fall back to the cardinal step with more
                    // remaining distance, keeping the Bresenham error balanced.
                    if (dx >= dy)
                    {
                        err -= dy; x0 += sx;   // cardinal X
                    }
                    else
                    {
                        err += dx; y0 += sy;   // cardinal Y
                    }
                }
            }
            else if (bStepX)
            {
                err -= dy; x0 += sx;
            }
            else if (bStepY)
            {
                err += dx; y0 += sy;
            }

            FIntPoint Pt(x0, y0);
            if (!Sub->IsCellWalkable(Pt))
            {
                // Hit an unwalkable cell — abort this Bresenham segment
                bSegmentClear = false;
                break;
            }
            if (OutPath.IsEmpty() || OutPath.Last() != Pt)
            {
                OutPath.Add(Pt);
            }
        }

        if (!bSegmentClear)
        {
            // Bresenham 段被阻断，直接添加 NavPath 途经点（NavMesh 已绕行障碍）
            if (Sub->IsCellWalkable(NextCell) &&
                (OutPath.IsEmpty() || OutPath.Last() != NextCell))
            {
                OutPath.Add(NextCell);
            }
        }
        // ─────────────────────────────────────────────────────────────────────

        PrevCell = NextCell;
    }

    return OutPath.Num() > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// StartNextStep
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::StartNextStep()
{
    UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return;

    FIntPoint NextCell = GridPath[PathIndex];

    // 动态障碍保护：每步出发前检查目标格当前通行性
    if (!Sub->IsCellWalkable(NextCell))
    {
        SetMoveState(EGridMoveState::Blocked);
        OnPathBlocked.Broadcast(NextCell);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(101, 3.f, FColor::Orange,
                FString::Printf(TEXT("[Move] Path blocked at (%d,%d)"),
                    NextCell.X, NextCell.Y));
        }
        return;
    }

    FGridCell NextCellData;
    Sub->GetCell(NextCell, NextCellData);

    StepStartPos  = GetOwner()->GetActorLocation();
    // WorldLocation.Z 是地表高度，需加胶囊半高使角色中心站在地面上而非嵌入地面
    StepTargetPos = NextCellData.WorldLocation + FVector(0.f, 0.f, CachedZOffset);
    StepAlpha     = 0.f;

    // 速度受地形倍率缩放；SpeedMultiplier 下限 0.01 防止除零
    float Speed = GetBaseSpeed() * FMath::Max(NextCellData.SpeedMultiplier, 0.01f);

    // 时长 = 实际世界距离 / 速度
    // 使用真实距离而非假设的"1格"，确保 Bresenham 回退到 NavMesh 途经点时
    // 跨多格的跳跃也以匀速完成，不会因距离与时长不匹配而出现瞬移
    float WorldDist  = FVector::Dist(StepStartPos, StepTargetPos);
    StepDuration = WorldDist / Speed;
}

// ─────────────────────────────────────────────────────────────────────────────
// FinishMovement
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::FinishMovement()
{
    // 精确对齐到终点格中心，消除浮点插值累积的微小偏差
    UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
    if (Sub)
    {
        FGridCell FinalCell;
        if (Sub->GetCell(CurrentCell, FinalCell))
        {
            FVector FinalPos = FinalCell.WorldLocation + FVector(0.f, 0.f, CachedZOffset);
            GetOwner()->SetActorLocation(FinalPos, false, nullptr,
                                         ETeleportType::TeleportPhysics);
        }
    }

    SetMoveState(EGridMoveState::Arrived);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(101, 2.f, FColor::Green,
            FString::Printf(TEXT("[Move] Arrived at (%d,%d)"),
                CurrentCell.X, CurrentCell.Y));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SetMoveState
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::SetMoveState(EGridMoveState NewState)
{
    // 相同状态不广播，避免下游逻辑被冗余触发
    if (MoveState == NewState) return;
    MoveState = NewState;
    OnMovementStateChanged.Broadcast(NewState);
}

// ─────────────────────────────────────────────────────────────────────────────
// StopMovement
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::StopMovement()
{
    GridPath.Empty();
    PathIndex = 0;
    StepAlpha = 0.f;
    SetMoveState(EGridMoveState::Idle);
}

// ─────────────────────────────────────────────────────────────────────────────
// StopAtNearestCell
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::StopAtNearestCell()
{
    // 原地停下，不做任何位移，避免视觉上的"倒退"或"前跳"
    // 将 CurrentCell 更新为当前实际位置对应的格子，保证后续寻路起点正确
    if (UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>())
    {
        CurrentCell = Sub->WorldToGrid(GetOwner()->GetActorLocation());
    }

    GridPath.Empty();
    PathIndex = 0;
    StepAlpha = 0.f;


    SetMoveState(EGridMoveState::Idle);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetCurrentCell
// ─────────────────────────────────────────────────────────────────────────────

FIntPoint UGridMovementComponent::GetCurrentCell() const
{
    return CurrentCell;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetCellData
// ─────────────────────────────────────────────────────────────────────────────

bool UGridMovementComponent::GetCellData(FIntPoint Coord, FGridCell& OutCell) const
{
    // 完全委托给 GridMapSubsystem，避免在组件层面缓存冗余副本
    UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return false;
    return Sub->GetCell(Coord, OutCell);
}

// ─────────────────────────────────────────────────────────────────────────────
// IsCellWalkable
// ─────────────────────────────────────────────────────────────────────────────

bool UGridMovementComponent::IsCellWalkable(FIntPoint Coord) const
{
    UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return false;
    return Sub->IsCellWalkable(Coord);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetMoveState
// ─────────────────────────────────────────────────────────────────────────────

EGridMoveState UGridMovementComponent::GetMoveState() const
{
    return MoveState;
}

FIntPoint UGridMovementComponent::GetTargetCell() const
{
    return TargetCell;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetDirectionString
// ─────────────────────────────────────────────────────────────────────────────

FString UGridMovementComponent::GetDirectionString(const FIntPoint& From,
                                                    const FIntPoint& To) const
{
    // 将两格差值 Clamp 到 [-1, 1] 后逐一匹配八方向
    int32 dx = FMath::Clamp(To.X - From.X, -1, 1);
    int32 dy = FMath::Clamp(To.Y - From.Y, -1, 1);

    if (dx ==  1 && dy ==  0) return TEXT("E");
    if (dx == -1 && dy ==  0) return TEXT("W");
    if (dx ==  0 && dy ==  1) return TEXT("N");
    if (dx ==  0 && dy == -1) return TEXT("S");
    if (dx ==  1 && dy ==  1) return TEXT("NE");
    if (dx == -1 && dy ==  1) return TEXT("NW");
    if (dx ==  1 && dy == -1) return TEXT("SE");
    if (dx == -1 && dy == -1) return TEXT("SW");
    return TEXT("?");
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawDebugInfo
// ─────────────────────────────────────────────────────────────────────────────

void UGridMovementComponent::DrawDebugInfo() const
{
    UWorld*            World = GetWorld();
    UGridMapSubsystem* Sub   = World ? World->GetSubsystem<UGridMapSubsystem>() : nullptr;
    if (!World || !Sub || !GEngine) return;

    // 格子高亮盒子的半尺寸（内缩 3cm 避免与格线重叠）
    const float   BoxZ   = 8.f;
    const FVector Extent(UGridMapSubsystem::CellSize * 0.5f - 3.f,
                         UGridMapSubsystem::CellSize * 0.5f - 3.f,
                         BoxZ);

    // 当前格：青色方框
    FVector CurWorld = Sub->GridToWorld(CurrentCell) + FVector(0.f, 0.f, BoxZ);
    DrawDebugBox(World, CurWorld, Extent, FQuat::Identity,
                 FColor::Cyan, false, -1.f, 0, 3.f);

    // 目标格：紫色方框（仅在寻路/移动阶段绘制）
    if (MoveState == EGridMoveState::Moving || MoveState == EGridMoveState::Pathfinding)
    {
        FVector TgtWorld = Sub->GridToWorld(TargetCell) + FVector(0.f, 0.f, BoxZ);
        DrawDebugBox(World, TgtWorld, Extent, FQuat::Identity,
                     FColor(160, 32, 240), false, -1.f, 0, 3.f);
    }

    // 路径：从当前步骤索引起，用黄色箭头连接各格，并在箭头终点标注序号
    // bPersistentLines=false + LifeTime=-1：每帧自动刷新，不与 P0 的持久格子叠加
    for (int32 i = PathIndex; i < GridPath.Num(); ++i)
    {
        FVector From = (i == PathIndex)
                       ? GetOwner()->GetActorLocation()
                       : Sub->GridToWorld(GridPath[i - 1]) + FVector(0.f, 0.f, BoxZ);
        FVector To   = Sub->GridToWorld(GridPath[i]) + FVector(0.f, 0.f, BoxZ);

        DrawDebugDirectionalArrow(World, From, To, 30.f, FColor::Yellow, false, -1.f, 0, 2.f);
        DrawDebugString(World, To + FVector(0.f, 0.f, 10.f),
                        FString::FromInt(i), nullptr, FColor::Yellow, 0.f, false, 1.f);
    }

    // ── 屏幕 Log（固定 Key，不堆叠；Key 从 102 起，不与 P0 的 Key=100 冲突）──

    // 状态字符串
    FString StateStr;
    switch (MoveState)
    {
        case EGridMoveState::Idle:        StateStr = TEXT("Idle");        break;
        case EGridMoveState::Pathfinding: StateStr = TEXT("Pathfinding"); break;
        case EGridMoveState::Moving:      StateStr = TEXT("Moving");      break;
        case EGridMoveState::Arrived:     StateStr = TEXT("Arrived");     break;
        case EGridMoveState::Blocked:     StateStr = TEXT("Blocked");     break;
        default:                          StateStr = TEXT("Unknown");     break;
    }

    // 当前移动方向（仅 Moving 状态且路径未耗尽时有意义）
    FString DirStr = (MoveState == EGridMoveState::Moving && PathIndex < GridPath.Num())
                     ? GetDirectionString(CurrentCell, GridPath[PathIndex])
                     : TEXT("-");

    // 第一行：状态、格坐标变化、剩余步数、插值进度、方向
    GEngine->AddOnScreenDebugMessage(102, 0.f, FColor::White,
        FString::Printf(
            TEXT("[Move] State:%s | Cell:(%d,%d)→(%d,%d) | Steps:%d | Alpha:%.2f | Dir:%s"),
            *StateStr,
            CurrentCell.X, CurrentCell.Y,
            TargetCell.X,  TargetCell.Y,
            GridPath.Num() - PathIndex,
            StepAlpha,
            *DirStr));

    // 第二行：速度、当前步时长、路径总长
    GEngine->AddOnScreenDebugMessage(103, 0.f, FColor::White,
        FString::Printf(
            TEXT("[Move] Speed:%.0fcm/s | StepDur:%.2fs | PathTotal:%d"),
            GetBaseSpeed(),
            StepDuration,
            GridPath.Num()));
}

// ─────────────────────────────────────────────────────────────────────────────
// 辅助函数
// ─────────────────────────────────────────────────────────────────────────────

float UGridMovementComponent::GetBaseSpeed() const
{
    // Config 为 null 或 BaseSpeed 无效时，降级到内置默认值 300 cm/s
    return (Config && Config->BaseSpeed > 0.f) ? Config->BaseSpeed : 300.f;
}

EGridDiagonalRule UGridMovementComponent::GetDiagonalRule() const
{
    return Config ? Config->DiagonalRule : EGridDiagonalRule::Strict;
}

bool UGridMovementComponent::IsDiagonalAllowed(const FIntPoint& From, const FIntPoint& To,
                                                const UGridMapSubsystem* Sub) const
{
    if (!Sub) return false;

    const EGridDiagonalRule Rule = GetDiagonalRule();
    if (Rule == EGridDiagonalRule::Disabled) return false;

    // The two cardinal neighbours that share the diagonal's corner
    const FIntPoint SideA(To.X, From.Y);   // step X first
    const FIntPoint SideB(From.X, To.Y);   // step Y first

    const bool bSideAWalkable = Sub->IsCellWalkable(SideA);
    const bool bSideBWalkable = Sub->IsCellWalkable(SideB);

    if (Rule == EGridDiagonalRule::Strict)
    {
        // Both cardinal neighbours must be walkable to prevent corner-cutting
        return bSideAWalkable && bSideBWalkable;
    }

    // Lenient: at least one side walkable
    return bSideAWalkable || bSideBWalkable;
}
