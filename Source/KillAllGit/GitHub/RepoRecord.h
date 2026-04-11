#pragma once

#include "CoreMinimal.h"
#include "RepoRecord.generated.h"

USTRUCT(BlueprintType)
struct FRepoRecord
{
	GENERATED_BODY()

	// Identity
	UPROPERTY()
	FString Owner;

	UPROPERTY()
	FString Name;

	// API metadata
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

	// Zip metadata
	UPROPERTY()
	FString ZipSha256;

	UPROPERTY()
	int64 ZipSizeBytes = 0;

	UPROPERTY()
	FString DownloadedAt;

	bool HasZipData() const
	{
		return !ZipSha256.IsEmpty();
	}

	static FString MakeRecordId(const FString& InOwner, const FString& InName)
	{
		return (InOwner + TEXT("_") + InName).ToLower();
	}
};
