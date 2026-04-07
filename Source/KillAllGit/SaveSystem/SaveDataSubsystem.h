#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveDataTypes.h"
#include "SaveDataSubsystem.generated.h"

class UJsonFileSaveProvider;

UCLASS()
class USaveDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void InitializeForTest(const FString& TestSaveDirectory);

	UFUNCTION(BlueprintCallable, Category = "Save Data")
	bool HasSaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Save Data")
	FSaveDataPayload LoadSaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Save Data")
	bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson);

	UFUNCTION(BlueprintCallable, Category = "Save Data")
	bool DeleteSaveData();

	UFUNCTION(BlueprintCallable, Category = "Save Data")
	FSaveDataPayload GetCurrentSaveData() const;

	UFUNCTION(Exec)
	void ShowSaveData();

private:
	UPROPERTY()
	TObjectPtr<UJsonFileSaveProvider> Provider;

	mutable FSaveDataPayload CachedPayload;
	mutable bool bHasCachedPayload = false;

	void CreateProvider(const FString& SaveDirectory = FString());
};
