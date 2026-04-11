#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameLevelPlayerController.generated.h"

class UInputMappingContext;

UCLASS(abstract)
class AGameLevelPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category="Input")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
};
