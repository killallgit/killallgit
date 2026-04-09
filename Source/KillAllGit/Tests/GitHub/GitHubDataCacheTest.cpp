#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GitHubDataCache.h"

namespace GitHubDataCacheTestUtils
{
	FString GetTestCacheDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("TestGitHubCache");
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_HasCache_ReturnsFalseWhenEmpty,
	"KillAllGit.GitHub.DataCache.HasCache_ReturnsFalseWhenEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_HasCache_ReturnsFalseWhenEmpty::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	TestFalse("Cache should not exist before write", Cache->HasCache(TEXT("microsoft/vscode")));

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_WriteThenHasCache,
	"KillAllGit.GitHub.DataCache.WriteThenHasCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_WriteThenHasCache::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	Cache->WriteCache(TEXT("microsoft/vscode"), TEXT("{\"name\":\"vscode\"}"));
	TestTrue("Cache should exist after write", Cache->HasCache(TEXT("microsoft/vscode")));

	CleanTestDir();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataCache_WriteThenRead,
	"KillAllGit.GitHub.DataCache.WriteThenRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataCache_WriteThenRead::RunTest(const FString& Parameters)
{
	using namespace GitHubDataCacheTestUtils;
	CleanTestDir();

	UGitHubDataCache* Cache = CreateTestCache();
	const FString Written = TEXT("{\"name\":\"vscode\",\"stars\":170000}");
	Cache->WriteCache(TEXT("microsoft/vscode"), Written);
	const FString Read = Cache->ReadCache(TEXT("microsoft/vscode"));
	TestEqual("Read content should match written content", Read, Written);

	CleanTestDir();
	return true;
}
