#include "GitHubAPIClient.h"
#include "KillAllGit.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

static const FString GitHubGraphQLUrl = TEXT("https://api.github.com/graphql");

void UGitHubAPIClient::FetchRepository(const FString& Owner, const FString& Name, FOnGitHubResponse OnComplete)
{
	const FString Token = ResolveAuthToken();
	if (Token.IsEmpty())
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubAPIClient] GITHUB_TOKEN not found in environment or .env file"));
		OnComplete.ExecuteIfBound(false, FString());
		return;
	}

	const FString Query = LoadQuery(TEXT("GetRepository"));
	if (Query.IsEmpty())
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubAPIClient] Failed to load GetRepository.graphql"));
		OnComplete.ExecuteIfBound(false, FString());
		return;
	}

	const FString Body = BuildRequestBody(Query, Owner, Name);

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GitHubGraphQLUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindUObject(
		this,
		&UGitHubAPIClient::OnRequestComplete,
		OnComplete
	);
	Request->ProcessRequest();
}

FString UGitHubAPIClient::ResolveAuthToken() const
{
	// Try shell environment first
	FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("GITHUB_TOKEN"));
	if (!Token.IsEmpty())
	{
		return Token;
	}

	// Fall back to .env in project root
	const FString EnvFilePath = FPaths::ProjectDir() / TEXT(".env");
	FString EnvContents;
	if (!FFileHelper::LoadFileToString(EnvContents, *EnvFilePath))
	{
		return FString();
	}

	TArray<FString> Lines;
	EnvContents.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key, Value;
		if (Line.Split(TEXT("="), &Key, &Value) && Key.TrimStartAndEnd() == TEXT("GITHUB_TOKEN"))
		{
			return Value.TrimStartAndEnd();
		}
	}

	return FString();
}

FString UGitHubAPIClient::LoadQuery(const FString& QueryName) const
{
	const FString QueryPath = FPaths::ProjectContentDir() / TEXT("Queries") / QueryName + TEXT(".graphql");
	FString QueryContents;
	if (!FFileHelper::LoadFileToString(QueryContents, *QueryPath))
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubAPIClient] Failed to load query file: %s"), *QueryPath);
	}
	return QueryContents;
}

FString UGitHubAPIClient::BuildRequestBody(const FString& Query, const FString& Owner, const FString& Name) const
{
	const TSharedPtr<FJsonObject> Variables = MakeShared<FJsonObject>();
	Variables->SetStringField(TEXT("owner"), Owner);
	Variables->SetStringField(TEXT("name"), Name);

	const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("query"), Query);
	Root->SetObjectField(TEXT("variables"), Variables);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Body;
}

void UGitHubAPIClient::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnGitHubResponse OnComplete)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubAPIClient] Request failed — no response"));
		OnComplete.ExecuteIfBound(false, FString());
		return;
	}

	const int32 StatusCode = Response->GetResponseCode();
	if (StatusCode != 200)
	{
		UE_LOG(LogKillAllGit, Error, TEXT("[GitHubAPIClient] HTTP %d: %s"), StatusCode, *Response->GetContentAsString());
		OnComplete.ExecuteIfBound(false, FString());
		return;
	}

	OnComplete.ExecuteIfBound(true, Response->GetContentAsString());
}
