#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveDataTypes.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UVerticalBox;
class USaveDataSubsystem;

UCLASS()
class UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	// Main menu panel — contains New Game and Continue
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> MainMenuBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContinueButton;

	// Variant picker panel — contains the three variant buttons
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VariantPickerBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CombatButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SideScrollingButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PlatformingButton;

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
