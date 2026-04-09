#include "MenuButton.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UMenuButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	SyncLabel();

	if (SizeBox_Root)
	{
		SizeBox_Root->SetMinDesiredWidth(MinWidth);
		SizeBox_Root->SetMinDesiredHeight(MinHeight);

		if (FixedWidth > 0.f)  SizeBox_Root->SetWidthOverride(FixedWidth);
		else                   SizeBox_Root->ClearWidthOverride();

		if (FixedHeight > 0.f) SizeBox_Root->SetHeightOverride(FixedHeight);
		else                   SizeBox_Root->ClearHeightOverride();
	}
}

void UMenuButton::NativeConstruct()
{
	Super::NativeConstruct();
	SyncLabel();
	Btn_Root->OnClicked.AddUniqueDynamic(this, &UMenuButton::HandleBtnClicked);
}

void UMenuButton::SetLabel(const FText& InLabel)
{
	ButtonLabel = InLabel;
	SyncLabel();
}

void UMenuButton::SyncLabel()
{
	if (Txt_Label)
	{
		Txt_Label->SetText(ButtonLabel);
	}
}

void UMenuButton::HandleBtnClicked()
{
	OnClicked.Broadcast();
}
