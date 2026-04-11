#include "InitOverlayWidget.h"
#include "GameInitPipeline.h"
#include "KillAllGit.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UInitOverlayWidget::BindToPipeline(UGameInitPipeline* Pipeline)
{
	if (!Pipeline)
	{
		return;
	}

	Pipeline->OnProgress.AddUObject(this, &UInitOverlayWidget::OnProgress);
	Pipeline->OnComplete.AddUObject(this, &UInitOverlayWidget::OnComplete);
	Pipeline->OnFailed.AddUObject(this, &UInitOverlayWidget::OnFailed);
}

void UInitOverlayWidget::OnProgress(const FString& StepLabel, float Fraction)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(Fraction);
	}
	if (Txt_StepLabel)
	{
		Txt_StepLabel->SetText(FText::FromString(StepLabel));
	}
}

void UInitOverlayWidget::OnComplete()
{
	RemoveFromParent();
}

void UInitOverlayWidget::OnFailed(const FString& Reason)
{
	UE_LOG(LogKillAllGit, Error, TEXT("[InitOverlay] Pipeline failed: %s"), *Reason);
	RemoveFromParent();
}
