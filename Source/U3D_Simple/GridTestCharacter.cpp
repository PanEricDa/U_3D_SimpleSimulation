// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.

#include "GridTestCharacter.h"
#include "GridMapSubsystem.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
// 构造函数
// ─────────────────────────────────────────────────────────────────────────────

AGridTestCharacter::AGridTestCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建网格移动组件，挂载至该 Actor
    GridMovement = CreateDefaultSubobject<UGridMovementComponent>(TEXT("GridMovement"));

    // Floating label above the head showing [AI] / [Manual]
    ModeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ModeLabel"));
    ModeLabel->SetupAttachment(RootComponent);
    ModeLabel->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
    ModeLabel->SetHorizontalAlignment(EHTA_Center);
    ModeLabel->SetVerticalAlignment(EVRTA_TextBottom);
    ModeLabel->SetWorldSize(20.f);
    ModeLabel->SetTextRenderColor(FColor::White);
    ModeLabel->SetText(FText::FromString(TEXT("[AI]")));
    ModeLabel->SetAbsolute(false, true, false);  // Absolute rotation — not affected by parent
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 绑定格子进入事件：每走完一格触发，用于屏幕显示当前格信息
    GridMovement->OnCellEntered.AddDynamic(this, &AGridTestCharacter::OnGridCellEntered);

    // 绑定状态变化事件：Arrived/Blocked 时 AI 模式自动续选目标
    GridMovement->OnMovementStateChanged.AddDynamic(this, &AGridTestCharacter::OnGridMoveStateChanged);

    if (ControlMode == EGridControlMode::AI)
    {
        // Delay 0.5 s to allow GridMapSubsystem to finish scanning before the first AI move
        GetWorldTimerManager().SetTimer(AIRetryTimerHandle, this,
            &AGridTestCharacter::PickRandomTarget, 0.5f, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── AI mode: timeout guard while Moving ────────────────────────────────
    if (ControlMode == EGridControlMode::AI && GridMovement->GetMoveState() == EGridMoveState::Moving)
    {
        MoveTimeoutTimer += DeltaTime;
        if (MoveTimeoutTimer >= MaxMoveTimeout)
        {
            UE_LOG(LogTemp, Warning, TEXT("[GridTestCharacter] Move timeout! Picking new target."));
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(104, 3.f, FColor::Red, TEXT("[AI] Timeout! Replanning..."));
            }
            MoveTimeoutTimer = 0.f;
            PickRandomTarget();
        }
    }
    else
    {
        MoveTimeoutTimer = 0.f;
    }

    // ── Manual mode: auto-revert to AI after idle timeout ────────────────
    if (ControlMode == EGridControlMode::Manual)
    {
        const float Timeout = (GridMovement->Config)
                              ? GridMovement->Config->ManualModeTimeout
                              : 0.f;

        if (Timeout > 0.f)
        {
            // Only accumulate while the character is idle (not executing a player command)
            if (GridMovement->GetMoveState() != EGridMoveState::Moving)
            {
                ManualIdleTimer += DeltaTime;
            }
            else
            {
                // Player issued a move command — reset the timer
                ManualIdleTimer = 0.f;
            }

            if (ManualIdleTimer >= Timeout)
            {
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(109, 3.f, FColor::Yellow,
                        TEXT("[Manual] Idle timeout — reverting to AI"));
                }
                SetControlMode(EGridControlMode::AI);
            }
        }
    }

    UpdateModeLabel();
}

// ─────────────────────────────────────────────────────────────────────────────
// PickRandomTarget
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::PickRandomTarget()
{
    // Only run in AI mode — Manual mode waits for player commands
    if (ControlMode != EGridControlMode::AI) return;

    UGridMapSubsystem* Sub = GetWorld()->GetSubsystem<UGridMapSubsystem>();
    if (!Sub) return;

    FIntPoint CurrentCell = GridMovement->GetCurrentCell();

    FIntPoint RandomCell;
    if (Sub->GetRandomWalkableCellInRadius(CurrentCell, WanderRadius * 100.f, RandomCell))
    {
        GridMovement->MoveToCell(RandomCell);
    }
    else
    {
        // No walkable cells available yet (map still scanning or fully blocked) — retry in 1 s
        // AIRetryTimerHandle is a member so it can be cancelled by SetControlMode(Manual)
        GetWorldTimerManager().SetTimer(AIRetryTimerHandle, this,
            &AGridTestCharacter::PickRandomTarget, 1.0f, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnGridMoveStateChanged
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::OnGridMoveStateChanged(EGridMoveState NewState)
{
    if (NewState == EGridMoveState::Arrived)
    {
        MoveTimeoutTimer = 0.f;

        // Manual mode: player decides the next move
        if (ControlMode == EGridControlMode::Manual) return;

        // AI mode: immediately pick the next random target
        PickRandomTarget();
    }
    else if (NewState == EGridMoveState::Blocked)
    {
        MoveTimeoutTimer = 0.f;

        // Manual mode: stay idle, inform player via on-screen message
        if (ControlMode == EGridControlMode::Manual) return;

        // AI mode: short delay before retry to avoid same-frame recursion
        GetWorldTimerManager().SetTimer(AIRetryTimerHandle, this,
            &AGridTestCharacter::PickRandomTarget, 0.1f, false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnGridCellEntered
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::OnGridCellEntered(FIntPoint Cell, FGridCell CellData)
{
    // 每次成功踏入新格，说明角色正在正常前进，重置超时计时器
    MoveTimeoutTimer = 0.f;

    // 踏入新格后在屏幕上显示当前格的关键属性（Key=105，2秒后自动消失）
    if (GEngine)
    {
        FString MatStr = CellData.MaterialTag.IsValid()
            ? CellData.MaterialTag.ToString()
            : TEXT("Default");
        GEngine->AddOnScreenDebugMessage(105, 2.f, FColor::White,
            FString::Printf(TEXT("[Cell] (%d,%d) Mat:%s | Cost:%.1f | SpeedMul:x%.2f | Walk:%s"),
                Cell.X, Cell.Y,
                *MatStr,
                CellData.MoveCost,
                CellData.SpeedMultiplier,
                CellData.bWalkable ? TEXT("true") : TEXT("false")));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SetControlMode
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::SetControlMode(EGridControlMode NewMode)
{
    ControlMode = NewMode;

    if (NewMode == EGridControlMode::Manual)
    {
        // Save the current AI destination so we can resume it after timeout
        if (GridMovement->GetMoveState() == EGridMoveState::Moving ||
            GridMovement->GetMoveState() == EGridMoveState::Pathfinding)
        {
            LastAITargetCell = GridMovement->GetTargetCell();
            bHasLastAITarget = true;
        }

        // Cancel any pending AI retry timers so they cannot fire into Manual state
        GetWorldTimerManager().ClearTimer(AIRetryTimerHandle);
        MoveTimeoutTimer = 0.f;
        ManualIdleTimer  = 0.f;

        // Stop movement and snap CurrentCell to the actor's actual world position
        GridMovement->StopAtNearestCell();
    }
    else // AI
    {
        MoveTimeoutTimer = 0.f;
        ManualIdleTimer  = 0.f;

        // Resume to the stored target if available, otherwise pick a random one
        if (bHasLastAITarget)
        {
            bHasLastAITarget = false;
            GridMovement->MoveToCell(LastAITargetCell);

            // If the stored target is no longer reachable, fall back to random
            if (GridMovement->GetMoveState() == EGridMoveState::Blocked ||
                GridMovement->GetMoveState() == EGridMoveState::Idle)
            {
                PickRandomTarget();
            }
        }
        else
        {
            PickRandomTarget();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawAIDebugInfo
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestCharacter::UpdateModeLabel()
{
    if (!ModeLabel) return;

    if (ControlMode == EGridControlMode::Manual)
    {
        ModeLabel->SetText(FText::FromString(TEXT("[Manual]")));
        ModeLabel->SetTextRenderColor(FColor::Green);
    }
    else
    {
        ModeLabel->SetText(FText::FromString(TEXT("[AI]")));
        ModeLabel->SetTextRenderColor(FColor::White);
    }

    // Face the camera (billboard)
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        FVector CamLoc;
        FRotator CamRot;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        FRotator LookAt = (CamLoc - ModeLabel->GetComponentLocation()).Rotation();
        ModeLabel->SetWorldRotation(LookAt);
    }
}
