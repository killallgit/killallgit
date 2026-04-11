#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RepoRecord.h"
#include "Interfaces/IHttpRequest.h"
#include "GameInitSubsystem.generated.h"

class UGameInitPipeline;
class UGitHubDataSubsystem;

DECLARE_DELEGATE(FOnGameInitComplete);

UCLASS()
class UGameInitSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BeginInit(const FString& Owner, const FString& Name, FOnGameInitComplete OnComplete);
	void CancelInit();

	UGameInitPipeline* GetPipeline() const { return Pipeline; }

private:
	UPROPERTY()
	TObjectPtr<UGameInitPipeline> Pipeline;

	FRepoRecord CurrentRecord;
	FString InitOwner;
	FString InitName;
	TSharedPtr<IHttpRequest> ActiveHttpRequest;

	UGitHubDataSubsystem* GetGitHubSubsystem() const;

	static void ExecuteCheckCache(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
		TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone);

	static void ExecuteDownloadZip(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
		TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone);

	static void ExecuteVerifyZip(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
		TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone);

	static void ExecuteSaveMetadata(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline,
		TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone);
};
