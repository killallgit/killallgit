#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveDataTypes.h"
#include "MainMenuWidget.generated.h"

class UEditableText;
class UMenuButton;
class UWidgetSwitcher;
class USaveDataSubsystem;

UCLASS()
class UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> MenuSwitcher;

	// Top-level panel (index 0 in MenuSwitcher)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_NewGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_Continue;

	// New Game panel
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> Input_Repo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_Refresh;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_Combat;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_SideScrolling;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMenuButton> Btn_Platforming;

	UFUNCTION()
	void OnNewGameClicked();

	UFUNCTION()
	void OnContinueClicked();

	UFUNCTION()
	void OnRefreshClicked();

	UFUNCTION()
	void OnSelectCombat();

	UFUNCTION()
	void OnSelectSideScrolling();

	UFUNCTION()
	void OnSelectPlatforming();

	void OnVariantSelected(EGameVariant Variant);
	void ShowTopLevel();
	void ShowNewGame();
	void StartGame(EGameVariant Variant, bool bIsNewGame);

	USaveDataSubsystem* GetSaveSubsystem() const;
};
