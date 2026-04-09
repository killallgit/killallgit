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
	const FString RepoId = Owner + TEXT("/") + Name;

	if (!bForceRefresh && Cache->HasCache(RepoId))
	{
		const FString Cached = Cache->ReadCache(RepoId);
		const FGitHubRepositoryData Data = ParseResponse(Cached);
		LogRepositoryData(Data);
		OnComplete.ExecuteIfBound(Data);
		return;
	}

	TWeakObjectPtr<UGitHubDataSubsystem> WeakThis(this);
	APIClient->FetchRepository(Owner, Name, FOnGitHubResponse::CreateLambda(
		[WeakThis, RepoId, OnComplete](bool bSuccess, FString JsonResponse)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (!bSuccess)
			{
				UE_LOG(LogKillAllGit, Error, TEXT("[GitHubDataSubsystem] Fetch failed for %s"), *RepoId);
				OnComplete.ExecuteIfBound(FGitHubRepositoryData{});
				return;
			}

			WeakThis->Cache->WriteCache(RepoId, JsonResponse);

			const FGitHubRepositoryData Data = ParseResponse(JsonResponse);
			LogRepositoryData(Data);
			OnComplete.ExecuteIfBound(Data);
		}
	));
}

FGitHubRepositoryData UGitHubDataSubsystem::ParseResponse(const FString& JsonString)
{
	FGitHubRepositoryData Data;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Data;
	}

	// Surface GraphQL-level errors (auth failure, rate limit, bad query, etc.)
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

	// primaryLanguage is an object: { "name": "TypeScript" }
	const TSharedPtr<FJsonObject>* LangObj;
	if ((*RepoObj)->TryGetObjectField(TEXT("primaryLanguage"), LangObj))
	{
		(*LangObj)->TryGetStringField(TEXT("name"), Data.PrimaryLanguage);
	}

	// defaultBranchRef is an object: { "name": "main" }
	const TSharedPtr<FJsonObject>* BranchObj;
	if ((*RepoObj)->TryGetObjectField(TEXT("defaultBranchRef"), BranchObj))
	{
		(*BranchObj)->TryGetStringField(TEXT("name"), Data.DefaultBranchName);
	}

	return Data;
}

void UGitHubDataSubsystem::LogRepositoryData(const FGitHubRepositoryData& Data)
{
	UE_LOG(LogKillAllGit, Display, TEXT("--- GitHub Repository Data ---"));
	UE_LOG(LogKillAllGit, Display, TEXT("name: %s"), *Data.Name);
	UE_LOG(LogKillAllGit, Display, TEXT("description: %s"), *Data.Description);
	UE_LOG(LogKillAllGit, Display, TEXT("url: %s"), *Data.Url);
	UE_LOG(LogKillAllGit, Display, TEXT("stars: %d"), Data.StargazerCount);
	UE_LOG(LogKillAllGit, Display, TEXT("forks: %d"), Data.ForkCount);
	UE_LOG(LogKillAllGit, Display, TEXT("language: %s"), *Data.PrimaryLanguage);
	UE_LOG(LogKillAllGit, Display, TEXT("default branch: %s"), *Data.DefaultBranchName);
	UE_LOG(LogKillAllGit, Display, TEXT("created: %s"), *Data.CreatedAt);
	UE_LOG(LogKillAllGit, Display, TEXT("updated: %s"), *Data.UpdatedAt);
	UE_LOG(LogKillAllGit, Display, TEXT("------------------------------"));
}
