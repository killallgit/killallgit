#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveDataTypes.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class USaveDataSubsystem;

UCLASS()
class UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY()
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY()
	TObjectPtr<UVerticalBox> VariantPickerBox;

	UFUNCTION()
	void OnNewGameClicked();

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnSelectCombat();

	UFUNCTION()
	void OnSelectSideScrolling();

	UFUNCTION()
	void OnSelectPlatforming();

	void OnVariantSelected(EGameVariant Variant);
	void ShowMainMenu();
	void ShowVariantPicker();
	void StartGame(EGameVariant Variant, bool bIsNewGame);

	USaveDataSubsystem* GetSaveSubsystem() const;
};
