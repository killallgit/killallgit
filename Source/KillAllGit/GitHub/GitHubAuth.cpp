#include "GitHubAuth.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString GitHubAuth::ResolveToken()
{
	FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("GITHUB_TOKEN"));
	if (!Token.IsEmpty())
	{
		return Token;
	}

	const FString EnvFilePath = FPaths::ProjectDir() / TEXT(".env");
	FString EnvContents;
	if (!FFileHelper::LoadFileToString(EnvContents, *EnvFilePath))
	{
		return FString();
	}

	TArray<FString> Lines;
	EnvContents.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key, Value;
		if (Line.Split(TEXT("="), &Key, &Value) && Key.TrimStartAndEnd() == TEXT("GITHUB_TOKEN"))
		{
			return Value.TrimStartAndEnd();
		}
	}

	return FString();
}
