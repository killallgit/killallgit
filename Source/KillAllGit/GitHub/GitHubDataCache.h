#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RepoRecord.h"
#include "GitHubDataCache.generated.h"

UCLASS()
class UGitHubDataCache : public UObject
{
	GENERATED_BODY()

public:
	bool HasRecord(const FString& Owner, const FString& Name) const;
	FRepoRecord ReadRecord(const FString& Owner, const FString& Name) const;
	void WriteRecord(const FRepoRecord& Record);

	void SetCacheDirectory(const FString& InCacheDir);

private:
	FString CacheDir;

	FString GetRecordPath(const FString& Owner, const FString& Name) const;
	static FString SanitizeKey(const FString& RepoId);
};
