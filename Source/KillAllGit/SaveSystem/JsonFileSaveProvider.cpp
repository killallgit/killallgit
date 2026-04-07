#include "JsonFileSaveProvider.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformFileManager.h"

void UJsonFileSaveProvider::SetSaveDirectory(const FString& InDirectory)
{
	SaveDirectory = InDirectory;
}

FString UJsonFileSaveProvider::GetSaveFilePath() const
{
	const FString Dir = SaveDirectory.IsEmpty()
		? FPaths::ProjectSavedDir() / TEXT("GameData")
		: SaveDirectory;
	return Dir / TEXT("save.json");
}

bool UJsonFileSaveProvider::HasSaveData() const
{
	return FPaths::FileExists(GetSaveFilePath());
}

FSaveDataPayload UJsonFileSaveProvider::LoadSaveData() const
{
	FSaveDataPayload Payload;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *GetSaveFilePath()))
	{
		return Payload;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return Payload;
	}

	Payload.Variant = GameVariantUtils::StringToVariant(
		JsonObject->GetStringField(TEXT("variant"))
	);

	const TSharedPtr<FJsonObject>* GitHubDataObject;
	if (JsonObject->TryGetObjectField(TEXT("githubData"), GitHubDataObject))
	{
		FString GitHubDataString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&GitHubDataString);
		FJsonSerializer::Serialize(GitHubDataObject->ToSharedRef(), Writer);
		Payload.GitHubDataJson = GitHubDataString;
	}

	FString CreatedAtString;
	if (JsonObject->TryGetStringField(TEXT("createdAt"), CreatedAtString))
	{
		FDateTime::ParseIso8601(*CreatedAtString, Payload.CreatedAt);
	}

	return Payload;
}

bool UJsonFileSaveProvider::CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("variant"), GameVariantUtils::VariantToString(Variant));
	RootObject->SetStringField(TEXT("createdAt"), FDateTime::UtcNow().ToIso8601());

	TSharedPtr<FJsonObject> GitHubDataObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GitHubDataJson);
	if (FJsonSerializer::Deserialize(Reader, GitHubDataObject) && GitHubDataObject.IsValid())
	{
		RootObject->SetObjectField(TEXT("githubData"), GitHubDataObject);
	}

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);

	const FString FilePath = GetSaveFilePath();
	const FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*Directory);

	return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool UJsonFileSaveProvider::DeleteSaveData()
{
	const FString FilePath = GetSaveFilePath();
	if (!FPaths::FileExists(FilePath))
	{
		return true;
	}
	return IFileManager::Get().Delete(*FilePath);
}
