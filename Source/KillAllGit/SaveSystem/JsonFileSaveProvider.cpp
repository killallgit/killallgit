#include "JsonFileSaveProvider.h"

void UJsonFileSaveProvider::SetSaveDirectory(const FString& InDirectory)
{
	SaveDirectory = InDirectory;
}

FString UJsonFileSaveProvider::GetSaveFilePath() const
{
	return FString();
}

bool UJsonFileSaveProvider::HasSaveData() const
{
	return false;
}

FSaveDataPayload UJsonFileSaveProvider::LoadSaveData() const
{
	return FSaveDataPayload();
}

bool UJsonFileSaveProvider::CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson)
{
	return false;
}

bool UJsonFileSaveProvider::DeleteSaveData()
{
	return false;
}
