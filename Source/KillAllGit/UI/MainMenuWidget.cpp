#include "MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "SaveDataSubsystem.h"
#include "MockGitHubData.h"
#include "Kismet/GameplayStatics.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ShowMainMenu();
}

USaveDataSubsystem* UMainMenuWidget::GetSaveSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UMainMenuWidget::ShowMainMenu()
{
	if (WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(WidgetTree->RootWidget);
	}

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuVBox"));
	WidgetTree->RootWidget = VBox;

	NewGameButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NewGameButton"));
	UTextBlock* NewGameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NewGameText"));
	NewGameText->SetText(FText::FromString(TEXT("New Game")));
	NewGameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NewGameButton->AddChild(NewGameText);
	NewGameButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnNewGameClicked);
	VBox->AddChild(NewGameButton);

	const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (SaveSubsystem && SaveSubsystem->HasSaveData())
	{
		ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
		UTextBlock* ContinueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueText"));
		ContinueText->SetText(FText::FromString(TEXT("Continue")));
		ContinueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ContinueButton->AddChild(ContinueText);
		ContinueButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnContinueClicked);
		VBox->AddChild(ContinueButton);
	}
}

void UMainMenuWidget::ShowVariantPicker()
{
	if (WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(WidgetTree->RootWidget);
	}

	VariantPickerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VariantPickerVBox"));
	WidgetTree->RootWidget = VariantPickerBox;

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VariantTitle"));
	Title->SetText(FText::FromString(TEXT("Select Variant")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	VariantPickerBox->AddChild(Title);

	auto AddVariantButton = [this](const FString& Label, FName HandlerName, FName ButtonName, FName TextName)
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
		BtnText->SetText(FText::FromString(Label));
		BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(BtnText);

		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, HandlerName);
		Btn->OnClicked.Add(Delegate);

		VariantPickerBox->AddChild(Btn);
	};

	AddVariantButton(TEXT("Combat"),
		GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectCombat),
		TEXT("CombatButton"), TEXT("CombatText"));
	AddVariantButton(TEXT("Side Scrolling"),
		GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectSideScrolling),
		TEXT("SideScrollingButton"), TEXT("SideScrollingText"));
	AddVariantButton(TEXT("Platforming"),
		GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectPlatforming),
		TEXT("PlatformingButton"), TEXT("PlatformingText"));
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
