#include "Misc/AutomationTest.h"
#include "GitHubDataSubsystem.h"
#include "RepoRecord.h"

namespace GitHubDataSubsystemTestFixtures
{
	const FString ValidResponse = TEXT(R"({
		"data": {
			"repository": {
				"name": "vscode",
				"description": "Visual Studio Code",
				"url": "https://github.com/microsoft/vscode",
				"stargazerCount": 170000,
				"forkCount": 30000,
				"createdAt": "2015-09-03T20:23:38Z",
				"updatedAt": "2026-04-09T00:00:00Z",
				"primaryLanguage": { "name": "TypeScript" },
				"defaultBranchRef": { "name": "main" }
			}
		}
	})");

	const FString ErrorResponse = TEXT(R"({
		"errors": [{ "message": "Bad credentials" }]
	})");

	const FString MissingDataResponse = TEXT(R"({ "something": "unexpected" })");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataSubsystem_ParseResponse_ValidJson,
	"KillAllGit.GitHub.DataSubsystem.ParseResponse_ValidJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataSubsystem_ParseResponse_ValidJson::RunTest(const FString& Parameters)
{
	const FRepoRecord Data = UGitHubDataSubsystem::ParseResponse(
		GitHubDataSubsystemTestFixtures::ValidResponse
	);

	TestEqual("Name", Data.Name, FString(TEXT("vscode")));
	TestEqual("Description", Data.Description, FString(TEXT("Visual Studio Code")));
	TestEqual("URL", Data.Url, FString(TEXT("https://github.com/microsoft/vscode")));
	TestEqual("Stars", Data.StargazerCount, 170000);
	TestEqual("Forks", Data.ForkCount, 30000);
	TestEqual("Language", Data.PrimaryLanguage, FString(TEXT("TypeScript")));
	TestEqual("Default branch", Data.DefaultBranchName, FString(TEXT("main")));
	TestEqual("CreatedAt", Data.CreatedAt, FString(TEXT("2015-09-03T20:23:38Z")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataSubsystem_ParseResponse_OwnerIsEmpty,
	"KillAllGit.GitHub.DataSubsystem.ParseResponse_OwnerIsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataSubsystem_ParseResponse_OwnerIsEmpty::RunTest(const FString& Parameters)
{
	const FRepoRecord Data = UGitHubDataSubsystem::ParseResponse(
		GitHubDataSubsystemTestFixtures::ValidResponse
	);

	TestTrue("Owner should be empty (caller sets it)", Data.Owner.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataSubsystem_ParseResponse_GraphQLErrors,
	"KillAllGit.GitHub.DataSubsystem.ParseResponse_GraphQLErrors_ReturnsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataSubsystem_ParseResponse_GraphQLErrors::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("GraphQL error: Bad credentials"), EAutomationExpectedErrorFlags::Contains);

	const FRepoRecord Data = UGitHubDataSubsystem::ParseResponse(
		GitHubDataSubsystemTestFixtures::ErrorResponse
	);

	TestTrue("Name should be empty on error", Data.Name.IsEmpty());
	TestEqual("Stars should be zero on error", Data.StargazerCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGitHubDataSubsystem_ParseResponse_MissingData,
	"KillAllGit.GitHub.DataSubsystem.ParseResponse_MissingData_ReturnsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FGitHubDataSubsystem_ParseResponse_MissingData::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Response missing 'data' field"), EAutomationExpectedErrorFlags::Contains);

	const FRepoRecord Data = UGitHubDataSubsystem::ParseResponse(
		GitHubDataSubsystemTestFixtures::MissingDataResponse
	);

	TestTrue("Name should be empty when data field missing", Data.Name.IsEmpty());

	return true;
}
