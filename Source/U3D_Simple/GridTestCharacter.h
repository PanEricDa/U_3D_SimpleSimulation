// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "GridMovementComponent.h"
#include "GridTestCharacter.generated.h"

/**
 * @enum EGridControlMode
 * @brief Control authority for AGridTestCharacter.
 *
 *   AI     — character continuously picks random walkable cells and moves autonomously.
 *   Manual — character waits for player-issued move commands; AI loop is suspended.
 *
 * Transitions are always immediate. There is no deferred/pending intermediate state.
 */
UENUM(BlueprintType)
enum class EGridControlMode : uint8
{
    AI     UMETA(DisplayName = "AI Random"),
    Manual UMETA(DisplayName = "Manual")
};

/**
 * @class AGridTestCharacter
 * @brief Grid movement test character - validates UGridMovementComponent AI wander and manual control.
 *
 * Control modes:
 *   - AI (default): automatically picks random walkable cells from UGridMapSubsystem and loops.
 *   - Manual: waits for external MoveToCell / MoveToWorldLocation commands.
 *
 * Safety:
 *   - Timeout guard: in AI mode, if Moving persists beyond MaxMoveTimeout seconds, forces a new target.
 *   - Deferred start: BeginPlay delays 0.5 s to allow GridMapSubsystem to finish scanning.
 *
 * @note Place this actor in a level that has UGridMapSubsystem deployed and scanned.
 */
UCLASS()
class U3D_SIMPLE_API AGridTestCharacter : public ACharacter
{
    GENERATED_BODY()

public:

    AGridTestCharacter();

    // ─────────────────────────────────────────────────────────────────────────
    // 组件
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 网格移动组件，负责寻路与逐格插值位移
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    TObjectPtr<UGridMovementComponent> GridMovement;

    /** Floating label above the character showing current control mode ([AI] / [Manual]). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid|Debug")
    TObjectPtr<UTextRenderComponent> ModeLabel;

    // ─────────────────────────────────────────────────────────────────────────
    // 配置
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief AI 卡死保护超时时长（秒）
     * @note  Moving 状态持续超过此值时，角色强制放弃当前路径并重新选目标
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|AI")
    float MaxMoveTimeout = 10.0f;

    /**
     * @brief AI 游走目标选取半径（单位：米）
     * @note  角色仅在以自身当前位置为圆心、此半径内的可通行格中随机选取终点。
     *        设为 0 时不限制范围，从全图所有可通行格中随机选取。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|AI", meta = (ClampMin = "0.0", ForceUnits = "m"))
    float WanderRadius = 0.f;

    // ─────────────────────────────────────────────────────────────────────────
    // Runtime state
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Current control mode (AI or Manual).
     * @note  Read-only from Blueprint. Use SetControlMode() to change.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Grid|AI")
    EGridControlMode ControlMode = EGridControlMode::AI;

    // ─────────────────────────────────────────────────────────────────────────
    // Public API
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Switches control mode immediately.
     * @param NewMode  Manual: stops movement and waits for player input.
     *                 AI:     cancels any pending timers and starts picking random targets.
     */
    UFUNCTION(BlueprintCallable, Category = "Grid|AI")
    void SetControlMode(EGridControlMode NewMode);

    // ─────────────────────────────────────────────────────────────────────────
    // ACharacter 覆写
    // ─────────────────────────────────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:

    // ─────────────────────────────────────────────────────────────────────────
    // Private runtime data
    // ─────────────────────────────────────────────────────────────────────────

    /** Timeout accumulator (seconds); incremented while AI mode + Moving state. */
    float MoveTimeoutTimer = 0.f;

    /** Accumulator for Manual-mode auto-revert timeout. Reset on every player command. */
    float ManualIdleTimer = 0.f;

    /** The AI destination cell that was active when the character switched to Manual mode. */
    FIntPoint LastAITargetCell;

    /** True if LastAITargetCell holds a valid target to resume after Manual mode ends. */
    bool bHasLastAITarget = false;

    /**
     * @brief Stored handle for the AI retry timer.
     * @note  Kept as a member so it can be cancelled when switching to Manual mode,
     *        preventing stale timer callbacks from firing into the wrong state.
     */
    FTimerHandle AIRetryTimerHandle;

    // ─────────────────────────────────────────────────────────────────────────
    // Private functions
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Picks a random walkable cell from UGridMapSubsystem and issues a move command.
     * @note  If no cells are available, retries after 1 second (stored in AIRetryTimerHandle).
     */
    void PickRandomTarget();

    /** Callback: fired each time the character enters a new grid cell. */
    UFUNCTION()
    void OnGridCellEntered(FIntPoint Cell, FGridCell CellData);

    /**
     * @brief Callback: fired when GridMovementComponent state changes.
     * @note  Arrived/Blocked in AI mode triggers the next target pick.
     *        Manual mode returns early — player decides the next command.
     */
    UFUNCTION()
    void OnGridMoveStateChanged(EGridMoveState NewState);

    /** Updates the mode label text and color above the character's head. */
    void UpdateModeLabel();
};
