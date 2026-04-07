#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SaveDataProvider.h"
#include "JsonFileSaveProvider.generated.h"

UCLASS()
class UJsonFileSaveProvider : public UObject, public ISaveDataProvider
{
	GENERATED_BODY()

public:
	void SetSaveDirectory(const FString& InDirectory);

	virtual bool HasSaveData() const override;
	virtual FSaveDataPayload LoadSaveData() const override;
	virtual bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson) override;
	virtual bool DeleteSaveData() override;

private:
	FString SaveDirectory;
	FString GetSaveFilePath() const;
};
