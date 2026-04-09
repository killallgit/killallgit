#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "MenuButton.generated.h"

class USizeBox;
class UTextBlock;

UCLASS(abstract)
class UMenuButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MenuButton")
	FText ButtonLabel;

	// 0 = unset (natural size from content)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MenuButton|Layout")
	float MinWidth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MenuButton|Layout")
	float MinHeight = 0.f;

	// Overrides natural size. 0 = unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MenuButton|Layout")
	float FixedWidth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MenuButton|Layout")
	float FixedHeight = 0.f;

	// Same delegate type as UButton::OnClicked — wire up identically
	UPROPERTY(BlueprintAssignable, Category="MenuButton")
	FOnButtonClickedEvent OnClicked;

	void SetLabel(const FText& InLabel);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_Root;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Root;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Label;

	UFUNCTION()
	void HandleBtnClicked();

	void SyncLabel();
};
