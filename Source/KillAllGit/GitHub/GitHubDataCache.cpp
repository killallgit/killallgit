#include "GitHubDataCache.h"
#include "KillAllGit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

FString UGitHubDataCache::SanitizeKey(const FString& RepoId)
{
	// "microsoft/vscode" → "microsoft_vscode"
	return RepoId.Replace(TEXT("/"), TEXT("_"));
}

void UGitHubDataCache::SetCacheDirectory(const FString& InCacheDir)
{
	CacheDir = InCacheDir;
}

FString UGitHubDataCache::GetCachePath(const FString& RepoId) const
{
	const FString Dir = CacheDir.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("GitHubCache") : CacheDir;
	return Dir / SanitizeKey(RepoId) + TEXT(".json");
}

bool UGitHubDataCache::HasCache(const FString& RepoId) const
{
	return IFileManager::Get().FileExists(*GetCachePath(RepoId));
}

FString UGitHubDataCache::ReadCache(const FString& RepoId) const
{
	FString Contents;
	FFileHelper::LoadFileToString(Contents, *GetCachePath(RepoId));
	return Contents;
}

void UGitHubDataCache::WriteCache(const FString& RepoId, const FString& JsonString)
{
	const FString CachePath = GetCachePath(RepoId);
	const FString Dir = FPaths::GetPath(CachePath);
	IFileManager::Get().MakeDirectory(*Dir, true);
	if (!FFileHelper::SaveStringToFile(JsonString, *CachePath))
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataCache] Failed to write cache: %s"), *CachePath);
	}
}
