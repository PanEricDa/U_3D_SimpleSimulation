// Copyright (c) 2025 U3D_Simple Project. All Rights Reserved.

#include "GridTestGameMode.h"
#include "GridTestPlayerController.h"

AGridTestGameMode::AGridTestGameMode()
{
	PlayerControllerClass = AGridTestPlayerController::StaticClass();

	// 不自动生成默认 Pawn，玩家通过点选场景中的 AGridTestCharacter 进行交互
	DefaultPawnClass = nullptr;
}
