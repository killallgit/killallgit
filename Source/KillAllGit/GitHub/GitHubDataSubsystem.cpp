#include "GitHubDataSubsystem.h"
#include "KillAllGit.h"
#include "GitHubAPIClient.h"
#include "GitHubDataCache.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UGitHubDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	APIClient = NewObject<UGitHubAPIClient>(this);
	Cache = NewObject<UGitHubDataCache>(this);
}

void UGitHubDataSubsystem::RequestRepositoryData(const FString& Owner, const FString& Name, bool bForceRefresh, FOnRepositoryDataReceived OnComplete)
{
	if (!bForceRefresh && Cache->HasRecord(Owner, Name))
	{
		const FRepoRecord Record = Cache->ReadRecord(Owner, Name);
		LogRepositoryData(Record);
		OnComplete.ExecuteIfBound(Record);
		return;
	}

	TWeakObjectPtr<UGitHubDataSubsystem> WeakThis(this);
	APIClient->FetchRepository(Owner, Name, FOnGitHubResponse::CreateLambda(
		[WeakThis, Owner, Name, OnComplete](bool bSuccess, FString JsonResponse)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (!bSuccess)
			{
				UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataSubsystem] Fetch failed for %s/%s"), *Owner, *Name);
				OnComplete.ExecuteIfBound(FRepoRecord{});
				return;
			}

			FRepoRecord Record = ParseResponse(JsonResponse);
			Record.Owner = Owner;
			Record.Name = Name;

			WeakThis->Cache->WriteRecord(Record);
			LogRepositoryData(Record);
			OnComplete.ExecuteIfBound(Record);
		}
	));
}

FRepoRecord UGitHubDataSubsystem::GetRepoRecord(const FString& Owner, const FString& Name) const
{
	if (Cache->HasRecord(Owner, Name))
	{
		return Cache->ReadRecord(Owner, Name);
	}
	return FRepoRecord{};
}

void UGitHubDataSubsystem::UpdateRepoRecord(const FRepoRecord& Record)
{
	FRepoRecord Existing = GetRepoRecord(Record.Owner, Record.Name);

	// Merge non-empty fields from incoming record onto existing
	if (!Record.Description.IsEmpty()) { Existing.Description = Record.Description; }
	if (!Record.Url.IsEmpty()) { Existing.Url = Record.Url; }
	if (Record.StargazerCount != 0) { Existing.StargazerCount = Record.StargazerCount; }
	if (Record.ForkCount != 0) { Existing.ForkCount = Record.ForkCount; }
	if (!Record.CreatedAt.IsEmpty()) { Existing.CreatedAt = Record.CreatedAt; }
	if (!Record.UpdatedAt.IsEmpty()) { Existing.UpdatedAt = Record.UpdatedAt; }
	if (!Record.PrimaryLanguage.IsEmpty()) { Existing.PrimaryLanguage = Record.PrimaryLanguage; }
	if (!Record.DefaultBranchName.IsEmpty()) { Existing.DefaultBranchName = Record.DefaultBranchName; }
	if (!Record.ZipSha256.IsEmpty()) { Existing.ZipSha256 = Record.ZipSha256; }
	if (Record.ZipSizeBytes != 0) { Existing.ZipSizeBytes = Record.ZipSizeBytes; }
	if (!Record.DownloadedAt.IsEmpty()) { Existing.DownloadedAt = Record.DownloadedAt; }

	Existing.Owner = Record.Owner;
	Existing.Name = Record.Name;

	Cache->WriteRecord(Existing);
}

bool UGitHubDataSubsystem::HasRepoZipData(const FString& Owner, const FString& Name) const
{
	if (!Cache->HasRecord(Owner, Name))
	{
		return false;
	}
	return Cache->ReadRecord(Owner, Name).HasZipData();
}

FRepoRecord UGitHubDataSubsystem::ParseResponse(const FString& JsonString)
{
	FRepoRecord Data;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Data;
	}

	const TArray<TSharedPtr<FJsonValue>>* Errors;
	if (Root->TryGetArrayField(TEXT("errors"), Errors))
	{
		for (const TSharedPtr<FJsonValue>& Error : *Errors)
		{
			const TSharedPtr<FJsonObject>* ErrorObj;
			FString Message;
			if (Error->TryGetObject(ErrorObj) && (*ErrorObj)->TryGetStringField(TEXT("message"), Message))
			{
				UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataSubsystem] GraphQL error: %s"), *Message);
			}
		}
		return Data;
	}

	const TSharedPtr<FJsonObject>* DataObj;
	if (!Root->TryGetObjectField(TEXT("data"), DataObj))
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataSubsystem] Response missing 'data' field"));
		return Data;
	}

	const TSharedPtr<FJsonObject>* RepoObj;
	if (!(*DataObj)->TryGetObjectField(TEXT("repository"), RepoObj))
	{
		return Data;
	}

	(*RepoObj)->TryGetStringField(TEXT("name"), Data.Name);
	(*RepoObj)->TryGetStringField(TEXT("description"), Data.Description);
	(*RepoObj)->TryGetStringField(TEXT("url"), Data.Url);
	(*RepoObj)->TryGetNumberField(TEXT("stargazerCount"), Data.StargazerCount);
	(*RepoObj)->TryGetNumberField(TEXT("forkCount"), Data.ForkCount);
	(*RepoObj)->TryGetStringField(TEXT("createdAt"), Data.CreatedAt);
	(*RepoObj)->TryGetStringField(TEXT("updatedAt"), Data.UpdatedAt);

	const TSharedPtr<FJsonObject>* LangObj;
	if ((*RepoObj)->TryGetObjectField(TEXT("primaryLanguage"), LangObj))
	{
		(*LangObj)->TryGetStringField(TEXT("name"), Data.PrimaryLanguage);
	}

	const TSharedPtr<FJsonObject>* BranchObj;
	if ((*RepoObj)->TryGetObjectField(TEXT("defaultBranchRef"), BranchObj))
	{
		(*BranchObj)->TryGetStringField(TEXT("name"), Data.DefaultBranchName);
	}

	return Data;
}

void UGitHubDataSubsystem::LogRepositoryData(const FRepoRecord& Data)
{
	UE_LOG(LogKillAllGit, Display, TEXT("--- GitHub Repository Data ---"));
	UE_LOG(LogKillAllGit, Display, TEXT("owner: %s"), *Data.Owner);
	UE_LOG(LogKillAllGit, Display, TEXT("name: %s"), *Data.Name);
	UE_LOG(LogKillAllGit, Display, TEXT("description: %s"), *Data.Description);
	UE_LOG(LogKillAllGit, Display, TEXT("url: %s"), *Data.Url);
	UE_LOG(LogKillAllGit, Display, TEXT("stars: %d"), Data.StargazerCount);
	UE_LOG(LogKillAllGit, Display, TEXT("forks: %d"), Data.ForkCount);
	UE_LOG(LogKillAllGit, Display, TEXT("language: %s"), *Data.PrimaryLanguage);
	UE_LOG(LogKillAllGit, Display, TEXT("default branch: %s"), *Data.DefaultBranchName);
	UE_LOG(LogKillAllGit, Display, TEXT("created: %s"), *Data.CreatedAt);
	UE_LOG(LogKillAllGit, Display, TEXT("updated: %s"), *Data.UpdatedAt);
	if (Data.HasZipData())
	{
		UE_LOG(LogKillAllGit, Display, TEXT("zip sha256: %s"), *Data.ZipSha256);
		UE_LOG(LogKillAllGit, Display, TEXT("zip size: %lld bytes"), Data.ZipSizeBytes);
		UE_LOG(LogKillAllGit, Display, TEXT("downloaded: %s"), *Data.DownloadedAt);
	}
	UE_LOG(LogKillAllGit, Display, TEXT("------------------------------"));
}
