#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "SaveDataSubsystem.h"
#include "SaveDataTypes.h"
#include "Engine/GameInstance.h"

namespace SaveDataSubsystemTestUtils
{
	FString GetTestSaveDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("TestSubsystemData");
	}

	void CleanTestDir()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteDirectoryRecursively(*GetTestSaveDir());
	}

	USaveDataSubsystem* CreateTestSubsystem()
	{
		UGameInstance* GI = NewObject<UGameInstance>();
		USaveDataSubsystem* Subsystem = NewObject<USaveDataSubsystem>(GI);
		Subsystem->InitializeForTest(GetTestSaveDir());
		return Subsystem;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSaveDataSubsystem_InitialState,
	"KillAllGit.SaveSystem.SaveDataSubsystem.InitialStateHasNoSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FSaveDataSubsystem_InitialState::RunTest(const FString& Parameters)
{
	using namespace SaveDataSubsystemTestUtils;
	CleanTestDir();

	USaveDataSubsystem* Subsystem = CreateTestSubsystem();
	TestFalse("Should have no save data initially", Subsystem->HasSaveData());

	Subsystem->DeleteSaveData();
	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSaveDataSubsystem_CreateAndLoad,
	"KillAllGit.SaveSystem.SaveDataSubsystem.CreateAndLoadRoundTrips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FSaveDataSubsystem_CreateAndLoad::RunTest(const FString& Parameters)
{
	using namespace SaveDataSubsystemTestUtils;
	CleanTestDir();

	USaveDataSubsystem* Subsystem = CreateTestSubsystem();

	const FString MockJson = TEXT(R"({"name":"test-repo"})");
	const bool Created = Subsystem->CreateSaveData(EGameVariant::Platforming, MockJson);
	TestTrue("CreateSaveData should succeed", Created);

	const FSaveDataPayload Payload = Subsystem->LoadSaveData();
	TestEqual("Variant should match", Payload.Variant, EGameVariant::Platforming);
	TestTrue("JSON should contain repo name", Payload.GitHubDataJson.Contains(TEXT("test-repo")));

	Subsystem->DeleteSaveData();
	CleanTestDir();
	return true;
}
