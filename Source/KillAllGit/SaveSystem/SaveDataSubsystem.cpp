#include "SaveDataSubsystem.h"
#include "JsonFileSaveProvider.h"
#include "Engine/Engine.h"

void USaveDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CreateProvider();
}

void USaveDataSubsystem::InitializeForTest(const FString& TestSaveDirectory)
{
	CreateProvider(TestSaveDirectory);
}

void USaveDataSubsystem::CreateProvider(const FString& SaveDirectory)
{
	Provider = NewObject<UJsonFileSaveProvider>(this);
	if (!SaveDirectory.IsEmpty())
	{
		Provider->SetSaveDirectory(SaveDirectory);
	}
	bHasCachedPayload = false;
}

bool USaveDataSubsystem::HasSaveData() const
{
	return Provider ? Provider->HasSaveData() : false;
}

FSaveDataPayload USaveDataSubsystem::LoadSaveData() const
{
	if (!Provider)
	{
		return FSaveDataPayload();
	}

	FSaveDataPayload Payload = Provider->LoadSaveData();

	CachedPayload = Payload;
	bHasCachedPayload = true;

	return Payload;
}

bool USaveDataSubsystem::CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson)
{
	if (!Provider)
	{
		return false;
	}

	const bool Result = Provider->CreateSaveData(Variant, GitHubDataJson);
	if (Result)
	{
		CachedPayload = Provider->LoadSaveData();
		bHasCachedPayload = true;
	}
	return Result;
}

bool USaveDataSubsystem::DeleteSaveData()
{
	if (!Provider)
	{
		return false;
	}

	const bool Result = Provider->DeleteSaveData();
	if (Result)
	{
		CachedPayload = FSaveDataPayload();
		bHasCachedPayload = false;
	}
	return Result;
}

FSaveDataPayload USaveDataSubsystem::GetCurrentSaveData() const
{
	if (bHasCachedPayload)
	{
		return CachedPayload;
	}
	return LoadSaveData();
}

void USaveDataSubsystem::ShowSaveData()
{
	if (!GEngine)
	{
		return;
	}

	if (!HasSaveData())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("No save data"));
		return;
	}

	const FSaveDataPayload Payload = GetCurrentSaveData();
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("Variant: %s | Created: %s"),
			*GameVariantUtils::VariantToString(Payload.Variant),
			*Payload.CreatedAt.ToString()));
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
		FString::Printf(TEXT("GitHub Data: %s"), *Payload.GitHubDataJson));
}
