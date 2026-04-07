#include "GameDataDebugWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "SaveDataSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UGameDataDebugWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugVBox"));
	WidgetTree->RootWidget = VBox;

	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DebugHeader"));
	Header->SetText(FText::FromString(TEXT("=== Game Data Debug ===")));
	Header->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	VBox->AddChild(Header);

	DebugTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DebugText"));
	DebugTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	VBox->AddChild(DebugTextBlock);

	RefreshDisplay();
}

void UGameDataDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshDisplay();
}

USaveDataSubsystem* UGameDataDebugWidget::GetSaveSubsystem() const
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UGameDataDebugWidget::RefreshDisplay()
{
	if (!DebugTextBlock)
	{
		return;
	}

	const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
	if (!SaveSubsystem || !SaveSubsystem->HasSaveData())
	{
		DebugTextBlock->SetText(FText::FromString(TEXT("No save data")));
		return;
	}

	const FSaveDataPayload Payload = SaveSubsystem->GetCurrentSaveData();

	const FString Display = FString::Printf(
		TEXT("Variant: %s\nCreated: %s\n\nGitHub Data:\n%s"),
		*GameVariantUtils::VariantToString(Payload.Variant),
		*Payload.CreatedAt.ToString(),
		*Payload.GitHubDataJson
	);

	DebugTextBlock->SetText(FText::FromString(Display));
}
