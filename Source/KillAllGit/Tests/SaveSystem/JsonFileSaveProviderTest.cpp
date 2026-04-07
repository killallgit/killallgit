#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "SaveDataTypes.h"
#include "JsonFileSaveProvider.h"

namespace JsonFileSaveProviderTestUtils
{
	FString GetTestSaveDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("TestGameData");
	}

	void CleanTestDir()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteDirectoryRecursively(*GetTestSaveDir());
	}

	UJsonFileSaveProvider* CreateTestProvider()
	{
		UJsonFileSaveProvider* Provider = NewObject<UJsonFileSaveProvider>();
		Provider->SetSaveDirectory(GetTestSaveDir());
		return Provider;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJsonSaveProvider_HasSaveData_ReturnsFalseWhenNoFile,
	"KillAllGit.SaveSystem.JsonFileSaveProvider.HasSaveData_ReturnsFalseWhenNoFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FJsonSaveProvider_HasSaveData_ReturnsFalseWhenNoFile::RunTest(const FString& Parameters)
{
	using namespace JsonFileSaveProviderTestUtils;
	CleanTestDir();

	UJsonFileSaveProvider* Provider = CreateTestProvider();
	TestFalse("No save file should exist", Provider->HasSaveData());

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJsonSaveProvider_CreateThenHas,
	"KillAllGit.SaveSystem.JsonFileSaveProvider.CreateThenHasSaveData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FJsonSaveProvider_CreateThenHas::RunTest(const FString& Parameters)
{
	using namespace JsonFileSaveProviderTestUtils;
	CleanTestDir();

	UJsonFileSaveProvider* Provider = CreateTestProvider();
	const bool Created = Provider->CreateSaveData(
		EGameVariant::Combat,
		TEXT(R"({"name":"test"})")
	);

	TestTrue("CreateSaveData should succeed", Created);
	TestTrue("HasSaveData should return true after create", Provider->HasSaveData());

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJsonSaveProvider_CreateThenLoad,
	"KillAllGit.SaveSystem.JsonFileSaveProvider.CreateThenLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FJsonSaveProvider_CreateThenLoad::RunTest(const FString& Parameters)
{
	using namespace JsonFileSaveProviderTestUtils;
	CleanTestDir();

	UJsonFileSaveProvider* Provider = CreateTestProvider();
	const FString MockJson = TEXT(R"({"name":"test","stars":42})");
	Provider->CreateSaveData(EGameVariant::SideScrolling, MockJson);

	const FSaveDataPayload Payload = Provider->LoadSaveData();

	TestEqual("Variant should match", Payload.Variant, EGameVariant::SideScrolling);
	TestTrue("GitHubDataJson should contain name", Payload.GitHubDataJson.Contains(TEXT("test")));
	TestTrue("CreatedAt should be set", Payload.CreatedAt > FDateTime::MinValue());

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FJsonSaveProvider_Delete,
	"KillAllGit.SaveSystem.JsonFileSaveProvider.DeleteRemovesFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FJsonSaveProvider_Delete::RunTest(const FString& Parameters)
{
	using namespace JsonFileSaveProviderTestUtils;
	CleanTestDir();

	UJsonFileSaveProvider* Provider = CreateTestProvider();
	Provider->CreateSaveData(EGameVariant::Combat, TEXT(R"({"name":"test"})"));
	TestTrue("Should have save data before delete", Provider->HasSaveData());

	const bool Deleted = Provider->DeleteSaveData();
	TestTrue("DeleteSaveData should succeed", Deleted);
	TestFalse("HasSaveData should return false after delete", Provider->HasSaveData());

	CleanTestDir();
	return true;
}
