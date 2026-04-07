#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "SaveDataSubsystem.h"
#include "MockGitHubData.h"
#include "Kismet/GameplayStatics.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	NewGameButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnNewGameClicked);
	ContinueButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnContinueClicked);
	CombatButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectCombat);
	SideScrollingButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectSideScrolling);
	PlatformingButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectPlatforming);

	ShowMainMenu();
}

USaveDataSubsystem* UMainMenuWidget::GetSaveSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UMainMenuWidget::ShowMainMenu()
{
	MainMenuBox->SetVisibility(ESlateVisibility::Visible);
	VariantPickerBox->SetVisibility(ESlateVisibility::Collapsed);

	const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	const bool bHasSave = SaveSubsystem && SaveSubsystem->HasSaveData();
	ContinueButton->SetVisibility(bHasSave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainMenuWidget::ShowVariantPicker()
{
	MainMenuBox->SetVisibility(ESlateVisibility::Collapsed);
	VariantPickerBox->SetVisibility(ESlateVisibility::Visible);
}

void UMainMenuWidget::OnNewGameClicked()
{
	ShowVariantPicker();
}

void UMainMenuWidget::OnContinueClicked()
{
	const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (!SaveSubsystem)
	{
		return;
	}

	const FSaveDataPayload Payload = SaveSubsystem->LoadSaveData();
	StartGame(Payload.Variant, false);
}

void UMainMenuWidget::OnSelectCombat()
{
	OnVariantSelected(EGameVariant::Combat);
}

void UMainMenuWidget::OnSelectSideScrolling()
{
	OnVariantSelected(EGameVariant::SideScrolling);
}

void UMainMenuWidget::OnSelectPlatforming()
{
	OnVariantSelected(EGameVariant::Platforming);
}

void UMainMenuWidget::OnVariantSelected(EGameVariant Variant)
{
	StartGame(Variant, true);
}

void UMainMenuWidget::StartGame(EGameVariant Variant, bool bIsNewGame)
{
	if (bIsNewGame)
	{
		USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
		if (SaveSubsystem)
		{
			SaveSubsystem->CreateSaveData(Variant, FMockGitHubData::GetMockJson());
		}
	}

	const FString MapPath = GameVariantUtils::GetMapPath(Variant);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*MapPath));
}
