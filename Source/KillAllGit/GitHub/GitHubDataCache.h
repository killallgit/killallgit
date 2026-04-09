#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GitHubDataCache.generated.h"

UCLASS()
class UGitHubDataCache : public UObject
{
	GENERATED_BODY()

public:
	bool HasCache(const FString& RepoId) const;
	FString ReadCache(const FString& RepoId) const;
	void WriteCache(const FString& RepoId, const FString& JsonString);

	/** Override the cache directory — used in tests to isolate from the real cache. */
	void SetCacheDirectory(const FString& InCacheDir);

private:
	FString CacheDir;

	FString GetCachePath(const FString& RepoId) const;
	static FString SanitizeKey(const FString& RepoId);
};
