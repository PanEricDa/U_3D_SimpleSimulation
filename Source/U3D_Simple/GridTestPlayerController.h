// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GridInputConfig.h"
#include "GridTestCharacter.h"
#include "InputActionValue.h"
#include "GridTestPlayerController.generated.h"

/**
 * @class AGridTestPlayerController
 * @brief Grid movement system player controller - dispatches select and move commands via Enhanced Input.
 *
 * Flow:
 *   1. BeginPlay injects IMC_Grid mapping context
 *   2. SetupInputComponent binds IA_GridClick (LMB) and IA_GridDeselect (RMB)
 *   3. HandleClick branches on SelectedCharacter validity:
 *        - No selection + hit character → SelectCharacter (switch to Manual mode)
 *        - Selection exists + hit ground → MoveToWorldLocation
 *        - Selection exists + hit character → ignored
 *   4. HandleDeselect clears selection and immediately restores AI mode
 *
 * Single-action design:
 *   One LMB action (IA_GridClick) replaces the former dual-binding.
 *   All branching is handled inside HandleClick with no inter-frame flags.
 */
UCLASS()
class U3D_SIMPLE_API AGridTestPlayerController : public APlayerController
{
    GENERATED_BODY()

public:

    // ─────────────────────────────────────────────────────────────────────────
    // 配置
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 网格输入配置资产
     * @note  需在编辑器中赋值，包含 IMC 与三个 IA 引用
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid|Input")
    TObjectPtr<UGridInputConfig> InputConfig;

    // ─────────────────────────────────────────────────────────────────────────
    // 运行时状态
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief 当前被选中的角色弱引用
     * @note  为空表示未选中任何角色；使用弱引用避免阻止 GC
     */
    UPROPERTY(BlueprintReadOnly, Category = "Grid|Input")
    TWeakObjectPtr<AGridTestCharacter> SelectedCharacter;

    // ─────────────────────────────────────────────────────────────────────────
    // APlayerController overrides
    // ─────────────────────────────────────────────────────────────────────────

    /** Injects IMC_Grid mapping context into the Enhanced Input subsystem. */
    virtual void BeginPlay() override;

    /** Binds IA_GridClick and IA_GridDeselect to their handler functions. */
    virtual void SetupInputComponent() override;

private:

    // ─────────────────────────────────────────────────────────────────────────
    // Input handlers
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Single LMB handler — branches on selection state.
     *
     * No selection: ray-cast (Visibility) → hit character → SelectCharacter.
     * Selection:    ray-cast (WorldStatic) → hit ground → MoveToWorldLocation.
     *               ray-cast hits character → ignored.
     */
    void HandleClick(const FInputActionValue& Value);

    /** RMB handler — clears selection and restores AI mode immediately. */
    void HandleDeselect(const FInputActionValue& Value);

    // ─────────────────────────────────────────────────────────────────────────
    // Selection management
    // ─────────────────────────────────────────────────────────────────────────

    /** Selects the given character and switches it to Manual control mode. */
    void SelectCharacter(AGridTestCharacter* InCharacter);

    /**
     * @brief Clears current selection and immediately switches character to AI mode.
     * @note  Safe to call with no active selection.
     */
    void DeselectCharacter();
};
