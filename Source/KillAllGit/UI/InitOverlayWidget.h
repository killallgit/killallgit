#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InitOverlayWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UGameInitPipeline;

UCLASS(Abstract)
class UInitOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToPipeline(UGameInitPipeline* Pipeline);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_StepLabel;

private:
	void OnProgress(const FString& StepLabel, float Fraction);
	void OnComplete();
	void OnFailed(const FString& Reason);
};
