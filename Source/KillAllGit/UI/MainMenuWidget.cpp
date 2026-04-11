#include "MainMenuWidget.h"
#include "KillAllGit.h"
#include "MenuButton.h"
#include "InitOverlayWidget.h"
#include "GameInitSubsystem.h"
#include "GameInitPipeline.h"
#include "Components/EditableText.h"
#include "Components/WidgetSwitcher.h"
#include "SaveDataSubsystem.h"
#include "GitHubDataSubsystem.h"
#include "RepoRecord.h"
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

UGameInitSubsystem* UMainMenuWidget::GetInitSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	return GI ? GI->GetSubsystem<UGameInitSubsystem>() : nullptr;
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

bool UMainMenuWidget::ParseRepoInput(FString& OutOwner, FString& OutName) const
{
	const FString RepoInput = Input_Repo->GetText().ToString();
	if (!RepoInput.Split(TEXT("/"), &OutOwner, &OutName))
	{
		UE_LOG(LogKillAllGit, Warning, TEXT("[MainMenu] Invalid repo format '%s' — expected owner/name"), *RepoInput);
		return false;
	}
	return true;
}

void UMainMenuWidget::OnRefreshClicked()
{
	FString Owner, Name;
	if (!ParseRepoInput(Owner, Name))
	{
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

void UMainMenuWidget::SetVariantButtonsEnabled(bool bEnabled)
{
	Btn_Combat->SetIsEnabled(bEnabled);
	Btn_SideScrolling->SetIsEnabled(bEnabled);
	Btn_Platforming->SetIsEnabled(bEnabled);
	Btn_Refresh->SetIsEnabled(bEnabled);
}

void UMainMenuWidget::StartGame(EGameVariant Variant, bool bIsNewGame)
{
	if (bIsNewGame)
	{
		if (bInitInProgress)
		{
			return;
		}

		FString Owner, Name;
		if (!ParseRepoInput(Owner, Name))
		{
			return;
		}

		if (!InitOverlayClass)
		{
			UE_LOG(LogKillAllGit, Error, TEXT("[MainMenu] InitOverlayClass not set — configure it in the Blueprint"));
			return;
		}

		UGameInitSubsystem* InitSubsystem = GetInitSubsystem();
		if (!InitSubsystem)
		{
			return;
		}

		PendingVariant = Variant;
		bInitInProgress = true;
		SetVariantButtonsEnabled(false);

		InitSubsystem->BeginInit(Owner, Name, FOnGameInitComplete::CreateUObject(this, &UMainMenuWidget::OnInitComplete));

		UGameInitPipeline* Pipeline = InitSubsystem->GetPipeline();
		if (Pipeline)
		{
			Pipeline->OnFailed.AddUObject(this, &UMainMenuWidget::OnInitFailed);
		}

		InitOverlay = CreateWidget<UInitOverlayWidget>(GetOwningPlayer(), InitOverlayClass);
		if (InitOverlay)
		{
			InitOverlay->AddToViewport(1);
			InitOverlay->BindToPipeline(Pipeline);
		}

		return;
	}

	const FString MapPath = GameVariantUtils::GetMapPath(Variant);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*MapPath));
}

void UMainMenuWidget::OnInitComplete()
{
	bInitInProgress = false;

	if (InitOverlay)
	{
		InitOverlay->RemoveFromParent();
		InitOverlay = nullptr;
	}

	USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (SaveSubsystem)
	{
		SaveSubsystem->CreateSaveData(PendingVariant, FMockGitHubData::GetMockJson());
	}

	const FString MapPath = GameVariantUtils::GetMapPath(PendingVariant);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*MapPath));
}

void UMainMenuWidget::OnInitFailed(const FString& Reason)
{
	bInitInProgress = false;

	if (InitOverlay)
	{
		InitOverlay->RemoveFromParent();
		InitOverlay = nullptr;
	}

	SetVariantButtonsEnabled(true);
}
