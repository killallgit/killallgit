// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatGameMode.h"
#include "UObject/ConstructorHelpers.h"

ACombatGameMode::ACombatGameMode()
{
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClass(TEXT("/Game/Variant_Combat/Blueprints/BP_CombatPlayerController"));
	if (ControllerClass.Class)
	{
		PlayerControllerClass = ControllerClass.Class;
	}
}