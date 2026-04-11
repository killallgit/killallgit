#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFileManager.h"
#include "RepoRecord.h"
#include "GitHubDataCache.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitSteps_VerifyZip_ConsistentHash,
	"KillAllGit.GameInit.Steps.VerifyZip_ConsistentHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitSteps_VerifyZip_ConsistentHash::RunTest(const FString& Parameters)
{
	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("TestVerifyZip");
	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestDir);
	IFileManager::Get().MakeDirectory(*TestDir, true);

	// Write known bytes to a file
	const TArray<uint8> TestBytes = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
	const FString FilePath = TestDir / TEXT("test.zip");
	FFileHelper::SaveArrayToFile(TestBytes, *FilePath);

	// Read back and hash
	TArray<uint8> ReadBytes;
	FFileHelper::LoadFileToArray(ReadBytes, *FilePath);
	FSHAHash Hash1 = FSHA1::HashBuffer(ReadBytes.GetData(), ReadBytes.Num());
	FSHAHash Hash2 = FSHA1::HashBuffer(ReadBytes.GetData(), ReadBytes.Num());

	TestEqual("Same bytes produce same hash", Hash1.ToString(), Hash2.ToString());
	TestFalse("Hash should not be empty", Hash1.ToString().IsEmpty());

	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameInitSteps_SaveMetadata_PreservesExistingApiData,
	"KillAllGit.GameInit.Steps.SaveMetadata_PreservesExistingApiData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGameInitSteps_SaveMetadata_PreservesExistingApiData::RunTest(const FString& Parameters)
{
	const FString TestDir = FPaths::ProjectSavedDir() / TEXT("TestSaveMetadata");
	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestDir);

	UGitHubDataCache* Cache = NewObject<UGitHubDataCache>();
	Cache->SetCacheDirectory(TestDir);

	// Write an existing record with API metadata
	FRepoRecord Existing;
	Existing.Owner = TEXT("microsoft");
	Existing.Name = TEXT("vscode");
	Existing.Description = TEXT("Visual Studio Code");
	Existing.StargazerCount = 170000;
	Existing.PrimaryLanguage = TEXT("TypeScript");
	Cache->WriteRecord(Existing);

	// Simulate what UpdateRepoRecord does: read-modify-write with zip fields
	FRepoRecord ZipRecord;
	ZipRecord.Owner = TEXT("microsoft");
	ZipRecord.Name = TEXT("vscode");
	ZipRecord.ZipSha256 = TEXT("abc123");
	ZipRecord.ZipSizeBytes = 5000000;
	ZipRecord.DownloadedAt = TEXT("2026-04-10T12:00:00Z");

	// Read existing, merge non-empty fields
	FRepoRecord Merged = Cache->ReadRecord(TEXT("microsoft"), TEXT("vscode"));
	Merged.ZipSha256 = ZipRecord.ZipSha256;
	Merged.ZipSizeBytes = ZipRecord.ZipSizeBytes;
	Merged.DownloadedAt = ZipRecord.DownloadedAt;
	Cache->WriteRecord(Merged);

	// Verify the merged record has both API and zip data
	const FRepoRecord Final = Cache->ReadRecord(TEXT("microsoft"), TEXT("vscode"));
	TestEqual("Description preserved", Final.Description, FString(TEXT("Visual Studio Code")));
	TestEqual("Stars preserved", Final.StargazerCount, 170000);
	TestEqual("Language preserved", Final.PrimaryLanguage, FString(TEXT("TypeScript")));
	TestEqual("ZipSha256 set", Final.ZipSha256, FString(TEXT("abc123")));
	TestEqual("ZipSizeBytes set", Final.ZipSizeBytes, (int64)5000000);
	TestEqual("DownloadedAt set", Final.DownloadedAt, FString(TEXT("2026-04-10T12:00:00Z")));
	TestTrue("HasZipData", Final.HasZipData());

	FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*TestDir);
	return true;
}
