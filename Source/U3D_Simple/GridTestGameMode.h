// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GridTestGameMode.generated.h"

/**
 * @class AGridTestGameMode
 * @brief 网格移动测试专用 GameMode
 *
 * 唯一职责：将 PlayerControllerClass 指定为 AGridTestPlayerController，
 * 确保 PIE 启动时鼠标光标显示、点击事件生效。
 */
UCLASS()
class U3D_SIMPLE_API AGridTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AGridTestGameMode();
};
