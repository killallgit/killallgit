#include "GitHubDataCache.h"
#include "KillAllGit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"

FString UGitHubDataCache::SanitizeKey(const FString& RepoId)
{
	return RepoId.Replace(TEXT("/"), TEXT("_"));
}

void UGitHubDataCache::SetCacheDirectory(const FString& InCacheDir)
{
	CacheDir = InCacheDir;
}

FString UGitHubDataCache::GetRecordPath(const FString& Owner, const FString& Name) const
{
	const FString Dir = CacheDir.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("RepoData") : CacheDir;
	return Dir / SanitizeKey(Owner + TEXT("/") + Name) + TEXT(".json");
}

bool UGitHubDataCache::HasRecord(const FString& Owner, const FString& Name) const
{
	return IFileManager::Get().FileExists(*GetRecordPath(Owner, Name));
}

FRepoRecord UGitHubDataCache::ReadRecord(const FString& Owner, const FString& Name) const
{
	FRepoRecord Record;
	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *GetRecordPath(Owner, Name)))
	{
		FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Record);
	}
	return Record;
}

void UGitHubDataCache::WriteRecord(const FRepoRecord& Record)
{
	const FString RecordPath = GetRecordPath(Record.Owner, Record.Name);
	const FString Dir = FPaths::GetPath(RecordPath);
	IFileManager::Get().MakeDirectory(*Dir, true);

	FString JsonString;
	FJsonObjectConverter::UStructToJsonObjectString(Record, JsonString);

	if (!FFileHelper::SaveStringToFile(JsonString, *RecordPath))
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataCache] Failed to write record: %s"), *RecordPath);
	}
}
