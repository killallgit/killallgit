#pragma once

#include "CoreMinimal.h"
#include "GitHubTypes.generated.h"

USTRUCT(BlueprintType)
struct FGitHubRepositoryData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	FString Url;

	UPROPERTY()
	int32 StargazerCount = 0;

	UPROPERTY()
	int32 ForkCount = 0;

	UPROPERTY()
	FString CreatedAt;

	UPROPERTY()
	FString UpdatedAt;

	UPROPERTY()
	FString PrimaryLanguage;

	UPROPERTY()
	FString DefaultBranchName;
};
