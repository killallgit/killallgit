#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveDataTypes.h"
#include "SaveDataProvider.generated.h"

UINTERFACE(MinimalAPI)
class USaveDataProvider : public UInterface
{
	GENERATED_BODY()
};

class ISaveDataProvider
{
	GENERATED_BODY()

public:
	virtual bool HasSaveData() const = 0;
	virtual FSaveDataPayload LoadSaveData() const = 0;
	virtual bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson) = 0;
	virtual bool DeleteSaveData() = 0;
};
