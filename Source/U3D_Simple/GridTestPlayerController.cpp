// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.

#include "GridTestPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

// ─────────────────────────────────────────────────────────────────────────────
// APlayerController 覆写
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 显示鼠标光标并启用点击/悬停事件，使 GetHitResultUnderCursor 能正常工作
    bShowMouseCursor        = true;
    bEnableClickEvents      = true;
    bEnableMouseOverEvents  = true;

    // 允许鼠标与 UI 和游戏世界同时交互，防止光标被捕获后消失
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);

    // 将网格输入映射上下文注入 Enhanced Input 子系统（Priority = 0）
    if (InputConfig && InputConfig->IMC_Grid)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(InputConfig->IMC_Grid, 0);
        }
    }
}

void AGridTestPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // InputConfig 未配置时跳过绑定，避免空指针
    if (!InputConfig) return;

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC) return;

    // IA_GridClick: single LMB action — branches on selection state inside HandleClick
    if (InputConfig->IA_GridClick)
    {
        EIC->BindAction(InputConfig->IA_GridClick, ETriggerEvent::Started,
                        this, &AGridTestPlayerController::HandleClick);
    }

    // IA_GridDeselect: RMB — clears selection and restores AI immediately
    if (InputConfig->IA_GridDeselect)
    {
        EIC->BindAction(InputConfig->IA_GridDeselect, ETriggerEvent::Started,
                        this, &AGridTestPlayerController::HandleDeselect);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input handlers
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestPlayerController::HandleClick(const FInputActionValue& Value)
{
    // Auto-clear stale selection if the character reverted to AI (e.g. idle timeout)
    if (SelectedCharacter.IsValid() && SelectedCharacter->ControlMode != EGridControlMode::Manual)
    {
        SelectedCharacter.Reset();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(107, 2.f, FColor::Yellow,
                TEXT("[Input] Selection cleared (character reverted to AI)"));
        }
    }

    if (!SelectedCharacter.IsValid())
    {
        // No selection — trace under cursor on Visibility channel to pick characters
        FHitResult Hit;
        if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit)) return;

        AGridTestCharacter* HitChar = Cast<AGridTestCharacter>(Hit.GetActor());
        if (HitChar)
        {
            SelectCharacter(HitChar);
        }
    }
    else
    {
        // Character is selected — trace under cursor for WorldStatic *objects only*.
        // Object-type query naturally excludes Pawn capsules (object type ECC_Pawn),
        // so the selected character can never swallow the click.
        TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
        ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

        FHitResult Hit;
        if (!GetHitResultUnderCursorForObjects(ObjectTypes, false, Hit)) return;

        SelectedCharacter->GridMovement->MoveToWorldLocation(Hit.ImpactPoint);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(106, 2.f, FColor::Cyan,
                FString::Printf(TEXT("[Input] MoveTo World:(%.0f,%.0f,%.0f)"),
                    Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z));
        }
    }
}

void AGridTestPlayerController::HandleDeselect(const FInputActionValue& Value)
{
    DeselectCharacter();
}

// ─────────────────────────────────────────────────────────────────────────────
// 选中管理
// ─────────────────────────────────────────────────────────────────────────────

void AGridTestPlayerController::SelectCharacter(AGridTestCharacter* InCharacter)
{
    SelectedCharacter = InCharacter;

    // Switch to Manual mode — stops AI and waits for player commands
    InCharacter->SetControlMode(EGridControlMode::Manual);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(107, 99999.f, FColor::Green,
            TEXT("[Input] Character Selected | Mode: Manual | RMB to deselect"));
    }
}

void AGridTestPlayerController::DeselectCharacter()
{
    if (SelectedCharacter.IsValid())
    {
        // Only switch if still in Manual — avoids re-triggering AI logic
        // when the character already auto-reverted via idle timeout
        if (SelectedCharacter->ControlMode == EGridControlMode::Manual)
        {
            SelectedCharacter->SetControlMode(EGridControlMode::AI);
        }
        SelectedCharacter.Reset();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(107, 2.f, FColor::Yellow,
            TEXT("[Input] Deselected | Mode: AI_Random"));
    }
}
