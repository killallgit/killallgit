#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "GitHubAPIClient.generated.h"

DECLARE_DELEGATE_TwoParams(FOnGitHubResponse, bool /*bSuccess*/, FString /*JsonResponse*/);

UCLASS()
class UGitHubAPIClient : public UObject
{
	GENERATED_BODY()

public:
	void FetchRepository(const FString& Owner, const FString& Name, FOnGitHubResponse OnComplete);

private:
	FString ResolveAuthToken() const;
	FString LoadQuery(const FString& QueryName) const;
	FString BuildRequestBody(const FString& Query, const FString& Owner, const FString& Name) const;

	void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FOnGitHubResponse OnComplete);
};
