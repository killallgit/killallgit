#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GitHubDataCache.h"
#include "RepoRecord.h"

namespace GitHubDataCacheTestUtils
{
	FString GetTestCacheDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("TestRepoData");
	}

	void CleanTestDir()
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*GetTestCacheDir());
	}

	UGitHubDataCache* CreateTestCache()
	{
		UGitHubDataCache* Cache = NewObject<UGitHubDataCache>();
		Cache->SetCacheDirectory(GetTestCacheDir());
		return Cache;
	}

	FRepoRecord MakeTestRecord()
	{
		FRepoRecord Record;
		Record.Owner = TEXT("microsoft");
		Record.Name = TEXT("vscode");
		Record.Description = TEXT("Visual Studio Code");
		Record.Url = TEXT("https://github.com/microsoft/vscode");
		Record.StargazerCount = 170000;
		Record.ForkCount = 30000;
		Record.CreatedAt = TEXT("2015-09-03T20:23:38Z");
		Record.UpdatedAt = TEXT("2026-04-09T00:00:00Z");
		Record.PrimaryLanguage = TEXT("TypeScript");
		Record.DefaultBranchName = TEXT("main");
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_HasRecord_ReturnsFalseWhenEmpty,
	"KillAllGit.GitHub.DataCache.HasRecord_ReturnsFalseWhenEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_HasRecord_ReturnsFalseWhenEmpty::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	TestFalse("Record should not exist before write", Cache->HasRecord(TEXT("microsoft"), TEXT("vscode")));

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_WriteThenHasRecord,
	"KillAllGit.GitHub.DataCache.WriteThenHasRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_WriteThenHasRecord::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	Cache->WriteRecord(MakeTestRecord());
	TestTrue("Record should exist after write", Cache->HasRecord(TEXT("microsoft"), TEXT("vscode")));

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_WriteThenRead_RoundTripsAllFields,
	"KillAllGit.GitHub.DataCache.WriteThenRead_RoundTripsAllFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_WriteThenRead_RoundTripsAllFields::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	FRepoRecord Written = MakeTestRecord();
	Written.ZipSha256 = TEXT("abc123def456");
	Written.ZipSizeBytes = 5000000;
	Written.DownloadedAt = TEXT("2026-04-10T12:00:00Z");

	Cache->WriteRecord(Written);
	const FRepoRecord Read = Cache->ReadRecord(TEXT("microsoft"), TEXT("vscode"));

	TestEqual("Owner", Read.Owner, Written.Owner);
	TestEqual("Name", Read.Name, Written.Name);
	TestEqual("Description", Read.Description, Written.Description);
	TestEqual("Url", Read.Url, Written.Url);
	TestEqual("Stars", Read.StargazerCount, Written.StargazerCount);
	TestEqual("Forks", Read.ForkCount, Written.ForkCount);
	TestEqual("CreatedAt", Read.CreatedAt, Written.CreatedAt);
	TestEqual("UpdatedAt", Read.UpdatedAt, Written.UpdatedAt);
	TestEqual("Language", Read.PrimaryLanguage, Written.PrimaryLanguage);
	TestEqual("Branch", Read.DefaultBranchName, Written.DefaultBranchName);
	TestEqual("ZipSha256", Read.ZipSha256, Written.ZipSha256);
	TestEqual("ZipSizeBytes", Read.ZipSizeBytes, Written.ZipSizeBytes);
	TestEqual("DownloadedAt", Read.DownloadedAt, Written.DownloadedAt);

	CleanTestDir();
	return true;
}
