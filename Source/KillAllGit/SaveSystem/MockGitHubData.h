#pragma once

#include "CoreMinimal.h"

struct FMockGitHubData
{
	static FString GetMockJson()
	{
		return TEXT(R"({
    "name": "vscode",
    "description": "Visual Studio Code",
    "owner": "microsoft",
    "stargazerCount": 170000,
    "forkCount": 30000,
    "primaryLanguage": "TypeScript",
    "createdAt": "2015-09-03T20:23:38Z",
    "defaultBranchName": "main"
})");
	}
};
