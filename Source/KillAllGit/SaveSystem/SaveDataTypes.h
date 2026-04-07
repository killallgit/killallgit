#pragma once

#include "CoreMinimal.h"
#include "SaveDataTypes.generated.h"

UENUM(BlueprintType)
enum class EGameVariant : uint8
{
	Combat        UMETA(DisplayName = "Combat"),
	SideScrolling UMETA(DisplayName = "Side Scrolling"),
	Platforming   UMETA(DisplayName = "Platforming")
};

USTRUCT(BlueprintType)
struct FSaveDataPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EGameVariant Variant = EGameVariant::Combat;

	UPROPERTY(BlueprintReadOnly)
	FString GitHubDataJson;

	UPROPERTY(BlueprintReadOnly)
	FDateTime CreatedAt;
};

namespace GameVariantUtils
{
	inline FString GetMapPath(EGameVariant Variant)
	{
		switch (Variant)
		{
		case EGameVariant::Combat:
			return TEXT("/Game/Variant_Combat/Lvl_Combat");
		case EGameVariant::SideScrolling:
			return TEXT("/Game/Variant_SideScrolling/Lvl_SideScrolling");
		case EGameVariant::Platforming:
			return TEXT("/Game/Variant_Platforming/Lvl_Platforming");
		default:
			return TEXT("/Game/Variant_Combat/Lvl_Combat");
		}
	}

	inline FString VariantToString(EGameVariant Variant)
	{
		switch (Variant)
		{
		case EGameVariant::Combat:        return TEXT("Combat");
		case EGameVariant::SideScrolling: return TEXT("SideScrolling");
		case EGameVariant::Platforming:   return TEXT("Platforming");
		default:                          return TEXT("Combat");
		}
	}

	inline EGameVariant StringToVariant(const FString& Str)
	{
		if (Str == TEXT("SideScrolling")) return EGameVariant::SideScrolling;
		if (Str == TEXT("Platforming"))   return EGameVariant::Platforming;
		return EGameVariant::Combat;
	}
}
