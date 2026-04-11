#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameInitPipeline.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInitProgress, const FString& /*StepLabel*/, float /*Fraction*/);
DECLARE_MULTICAST_DELEGATE(FOnInitComplete);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInitFailed, const FString& /*Reason*/);

using FInitStepExecuteFunc = TFunction<void(TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)>;

struct FInitStepEntry
{
	FString Label;
	FInitStepExecuteFunc Execute;
};

UCLASS()
class UGameInitPipeline : public UObject
{
	GENERATED_BODY()

public:
	void AddStep(const FString& Label, FInitStepExecuteFunc ExecuteFunc);
	void Run();
	void Complete();
	void Fail(const FString& Reason);
	void Cancel();
	bool IsRunning() const;

	FOnInitProgress OnProgress;
	FOnInitComplete OnComplete;
	FOnInitFailed OnFailed;

private:
	TArray<FInitStepEntry> Steps;
	int32 CurrentStepIndex = -1;
	bool bAborted = false;
	bool bRunning = false;

	void RunNextStep();
};
