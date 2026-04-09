#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GitHubTypes.h"
#include "GitHubDataSubsystem.generated.h"

class UGitHubAPIClient;
class UGitHubDataCache;

DECLARE_DELEGATE_OneParam(FOnRepositoryDataReceived, const FGitHubRepositoryData& /*Data*/);

UCLASS()
class UGitHubDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void RequestRepositoryData(const FString& Owner, const FString& Name, bool bForceRefresh, FOnRepositoryDataReceived OnComplete);

	static FGitHubRepositoryData ParseResponse(const FString& JsonString);

private:
	UPROPERTY()
	TObjectPtr<UGitHubAPIClient> APIClient;

	UPROPERTY()
	TObjectPtr<UGitHubDataCache> Cache;

	static void LogRepositoryData(const FGitHubRepositoryData& Data);
};
