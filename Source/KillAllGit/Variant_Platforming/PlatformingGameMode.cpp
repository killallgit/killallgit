// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Platforming/PlatformingGameMode.h"
#include "UObject/ConstructorHelpers.h"

APlatformingGameMode::APlatformingGameMode()
{
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClass(TEXT("/Game/Variant_Platforming/Blueprints/BP_PlatformingPlayerController"));
	if (ControllerClass.Class)
	{
		PlayerControllerClass = ControllerClass.Class;
	}
}