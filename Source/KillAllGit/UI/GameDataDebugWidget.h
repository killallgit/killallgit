#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameDataDebugWidget.generated.h"

class UTextBlock;
class USaveDataSubsystem;

UCLASS()
class UGameDataDebugWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> DebugTextBlock;

	void RefreshDisplay();
	USaveDataSubsystem* GetSaveSubsystem() const;
};
