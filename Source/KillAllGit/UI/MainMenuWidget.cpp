#include "MainMenuWidget.h"
#include "KillAllGit.h"
#include "MenuButton.h"
#include "Components/EditableText.h"
#include "Components/WidgetSwitcher.h"
#include "SaveDataSubsystem.h"
#include "GitHubDataSubsystem.h"
#include "MockGitHubData.h"
#include "Kismet/GameplayStatics.h"

static const FString DefaultRepo = TEXT("microsoft/vscode");

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Btn_NewGame->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnNewGameClicked);
	Btn_Continue->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnContinueClicked);
	Btn_Refresh->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnRefreshClicked);
	Btn_Combat->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectCombat);
	Btn_SideScrolling->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectSideScrolling);
	Btn_Platforming->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSelectPlatforming);

	Input_Repo->SetText(FText::FromString(DefaultRepo));

	ShowTopLevel();
}

USaveDataSubsystem* UMainMenuWidget::GetSaveSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UMainMenuWidget::ShowTopLevel()
{
	MenuSwitcher->SetActiveWidgetIndex(0);

	const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	const bool bHasSave = SaveSubsystem && SaveSubsystem->HasSaveData();
	Btn_Continue->SetVisibility(bHasSave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMainMenuWidget::ShowNewGame()
{
	MenuSwitcher->SetActiveWidgetIndex(1);
}

void UMainMenuWidget::OnNewGameClicked()
{
	ShowNewGame();
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

void UMainMenuWidget::OnRefreshClicked()
{
	const FString RepoInput = Input_Repo->GetText().ToString();

	FString Owner, Name;
	if (!RepoInput.Split(TEXT("/"), &Owner, &Name))
	{
		UE_LOG(LogKillAllGit, Warning, TEXT("[MainMenu] Invalid repo format '%s' — expected owner/name"), *RepoInput);
		return;
	}

	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	if (!GI)
	{
		return;
	}

	UGitHubDataSubsystem* GitHubSubsystem = GI->GetSubsystem<UGitHubDataSubsystem>();
	if (!GitHubSubsystem)
	{
		return;
	}

	GitHubSubsystem->RequestRepositoryData(Owner, Name, true, FOnRepositoryDataReceived());
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
