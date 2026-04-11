#include "GameInitPipeline.h"
#include "KillAllGit.h"

void UGameInitPipeline::AddStep(const FString& Label, FInitStepExecuteFunc ExecuteFunc)
{
	Steps.Add(FInitStepEntry{Label, MoveTemp(ExecuteFunc)});
}

void UGameInitPipeline::Run()
{
	bRunning = true;
	bAborted = false;
	CurrentStepIndex = -1;
	RunNextStep();
}

void UGameInitPipeline::Complete()
{
	if (bAborted)
	{
		return;
	}
	bAborted = true;
	bRunning = false;
	OnComplete.Broadcast();
}

void UGameInitPipeline::Fail(const FString& Reason)
{
	if (bAborted)
	{
		return;
	}
	bAborted = true;
	bRunning = false;
	UE_LOG(LogKillAllGit, Error, TEXT("[GameInitPipeline] Failed: %s"), *Reason);
	OnFailed.Broadcast(Reason);
}

void UGameInitPipeline::Cancel()
{
	bAborted = true;
	bRunning = false;
}

bool UGameInitPipeline::IsRunning() const
{
	return bRunning;
}

void UGameInitPipeline::RunNextStep()
{
	if (bAborted)
	{
		return;
	}

	CurrentStepIndex++;

	if (CurrentStepIndex >= Steps.Num())
	{
		bRunning = false;
		OnComplete.Broadcast();
		return;
	}

	const FInitStepEntry& Step = Steps[CurrentStepIndex];
	const FString StepLabel = Step.Label;

	auto ReportProgress = [this, StepLabel](float Fraction)
	{
		if (!bAborted)
		{
			OnProgress.Broadcast(StepLabel, Fraction);
		}
	};

	auto OnStepDone = [this]()
	{
		if (!bAborted)
		{
			RunNextStep();
		}
	};

	Step.Execute(MoveTemp(ReportProgress), MoveTemp(OnStepDone));
}
