#include "Misc/AutomationTest.h"
#include "GameInitPipeline.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_EmptyPipeline_FiresComplete,
	"KillAllGit.GameInit.Pipeline.EmptyPipeline_FiresComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_EmptyPipeline_FiresComplete::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bCompleted = false;
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->Run();

	TestTrue("OnComplete should fire for empty pipeline", bCompleted);
	TestFalse("Pipeline should not be running after completion", Pipeline->IsRunning());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_SingleStep_RunsAndCompletes,
	"KillAllGit.GameInit.Pipeline.SingleStep_RunsAndCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_SingleStep_RunsAndCompletes::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bStepRan = false;
	bool bCompleted = false;

	Pipeline->AddStep(TEXT("Step1"), [&bStepRan](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
	{
		bStepRan = true;
		ReportProgress(1.0f);
		OnStepDone();
	});
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->Run();

	TestTrue("Step should have run", bStepRan);
	TestTrue("OnComplete should fire after single step", bCompleted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_ThreeSteps_RunInOrder,
	"KillAllGit.GameInit.Pipeline.ThreeSteps_RunInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_ThreeSteps_RunInOrder::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	TArray<FString> Order;
	bool bCompleted = false;

	for (int32 i = 1; i <= 3; i++)
	{
		const FString StepName = FString::Printf(TEXT("Step%d"), i);
		Pipeline->AddStep(StepName, [&Order, StepName](TFunction<void(float)>, TFunction<void()> OnStepDone)
		{
			Order.Add(StepName);
			OnStepDone();
		});
	}
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->Run();

	TestEqual("Three steps should run", Order.Num(), 3);
	TestEqual("First step", Order[0], FString(TEXT("Step1")));
	TestEqual("Second step", Order[1], FString(TEXT("Step2")));
	TestEqual("Third step", Order[2], FString(TEXT("Step3")));
	TestTrue("OnComplete fires", bCompleted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_ProgressDelegate_FiresCorrectly,
	"KillAllGit.GameInit.Pipeline.ProgressDelegate_FiresCorrectly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_ProgressDelegate_FiresCorrectly::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	FString ReceivedLabel;
	float ReceivedFraction = -1.0f;

	Pipeline->AddStep(TEXT("Downloading..."), [](TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)
	{
		ReportProgress(0.5f);
		OnStepDone();
	});
	Pipeline->OnProgress.AddLambda([&ReceivedLabel, &ReceivedFraction](const FString& Label, float Fraction)
	{
		ReceivedLabel = Label;
		ReceivedFraction = Fraction;
	});
	Pipeline->Run();

	TestEqual("Label matches step", ReceivedLabel, FString(TEXT("Downloading...")));
	TestEqual("Fraction matches reported value", ReceivedFraction, 0.5f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_Complete_SkipsRemainingSteps,
	"KillAllGit.GameInit.Pipeline.Complete_SkipsRemainingSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_Complete_SkipsRemainingSteps::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bStep2Ran = false;
	bool bCompleted = false;

	Pipeline->AddStep(TEXT("Step1"), [Pipeline](TFunction<void(float)>, TFunction<void()>)
	{
		Pipeline->Complete();
	});
	Pipeline->AddStep(TEXT("Step2"), [&bStep2Ran](TFunction<void(float)>, TFunction<void()> OnStepDone)
	{
		bStep2Ran = true;
		OnStepDone();
	});
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->Run();

	TestTrue("OnComplete should fire", bCompleted);
	TestFalse("Step2 should not run after Complete()", bStep2Ran);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_Fail_AbortsPipeline,
	"KillAllGit.GameInit.Pipeline.Fail_AbortsPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_Fail_AbortsPipeline::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bStep2Ran = false;
	bool bCompleted = false;
	FString FailReason;

	AddExpectedError(TEXT("Something broke"), EAutomationExpectedErrorFlags::Contains);

	Pipeline->AddStep(TEXT("Step1"), [Pipeline](TFunction<void(float)>, TFunction<void()>)
	{
		Pipeline->Fail(TEXT("Something broke"));
	});
	Pipeline->AddStep(TEXT("Step2"), [&bStep2Ran](TFunction<void(float)>, TFunction<void()> OnStepDone)
	{
		bStep2Ran = true;
		OnStepDone();
	});
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->OnFailed.AddLambda([&FailReason](const FString& Reason) { FailReason = Reason; });
	Pipeline->Run();

	TestEqual("Fail reason matches", FailReason, FString(TEXT("Something broke")));
	TestFalse("Step2 should not run after Fail()", bStep2Ran);
	TestFalse("OnComplete should not fire on failure", bCompleted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_Cancel_FiresNoDelegates,
	"KillAllGit.GameInit.Pipeline.Cancel_FiresNoDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_Cancel_FiresNoDelegates::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bCompleted = false;
	bool bFailed = false;

	Pipeline->AddStep(TEXT("Step1"), [Pipeline](TFunction<void(float)>, TFunction<void()>)
	{
		Pipeline->Cancel();
	});
	Pipeline->OnComplete.AddLambda([&bCompleted]() { bCompleted = true; });
	Pipeline->OnFailed.AddLambda([&bFailed](const FString&) { bFailed = true; });
	Pipeline->Run();

	TestFalse("OnComplete should not fire on cancel", bCompleted);
	TestFalse("OnFailed should not fire on cancel", bFailed);
	TestFalse("Pipeline should not be running after cancel", Pipeline->IsRunning());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitPipeline_StepDoneAfterFail_IsNoOp,
	"KillAllGit.GameInit.Pipeline.StepDoneAfterFail_IsNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitPipeline_StepDoneAfterFail_IsNoOp::RunTest(const FString& Parameters)
{
	UGameInitPipeline* Pipeline = NewObject<UGameInitPipeline>();
	bool bStep2Ran = false;
	TFunction<void()> CapturedDone;

	AddExpectedError(TEXT("Early fail"), EAutomationExpectedErrorFlags::Contains);

	Pipeline->AddStep(TEXT("Step1"), [Pipeline, &CapturedDone](TFunction<void(float)>, TFunction<void()> OnStepDone)
	{
		CapturedDone = OnStepDone;
		Pipeline->Fail(TEXT("Early fail"));
	});
	Pipeline->AddStep(TEXT("Step2"), [&bStep2Ran](TFunction<void(float)>, TFunction<void()> OnStepDone)
	{
		bStep2Ran = true;
		OnStepDone();
	});
	Pipeline->Run();

	// Call the captured OnStepDone after Fail — should be a no-op
	CapturedDone();

	TestFalse("Step2 should not run when OnStepDone called after Fail", bStep2Ran);

	return true;
}
