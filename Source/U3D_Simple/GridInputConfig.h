// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GridInputConfig.generated.h"

/**
 * @class UGridInputConfig
 * @brief Grid input config asset - centralizes Enhanced Input mapping and Action references.
 *
 * Usage:
 *   Create a DataAsset child blueprint, fill in the IMC and IA assets,
 *   then assign the asset reference to AGridTestPlayerController::InputConfig.
 *
 * Click model:
 *   IA_GridClick (LMB) is the single left-click action.
 *   The PlayerController branches internally:
 *     - No selection: LMB on character → Select
 *     - Selection exists: LMB on ground  → MoveToWorldLocation
 *   IA_GridDeselect (RMB) clears selection and restores AI mode immediately.
 */
UCLASS(BlueprintType)
class U3D_SIMPLE_API UGridInputConfig : public UDataAsset
{
    GENERATED_BODY()

public:

    // ─────────────────────────────────────────────────────────────────────────
    // 映射上下文
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 网格系统统一输入映射上下文
     * @note  在 BeginPlay 阶段由 Controller 注入 EnhancedInputLocalPlayerSubsystem
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Input")
    TObjectPtr<UInputMappingContext> IMC_Grid;

    // ─────────────────────────────────────────────────────────────────────────
    // Input Action
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Single left-click action (LMB).
     * @note  Replaces the former IA_GridSelect + IA_GridMoveTo dual-binding.
     *        The PlayerController decides behavior based on current selection state.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Input")
    TObjectPtr<UInputAction> IA_GridClick;

    /**
     * @brief Deselect action (RMB).
     * @note  Clears selection and immediately restores AI random-wander mode.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Input")
    TObjectPtr<UInputAction> IA_GridDeselect;
};
