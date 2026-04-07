#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKillAllGitSmokeTest,
	"KillAllGit.Smoke.TrueIsTrue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FKillAllGitSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue("Sanity check", true);
	return true;
}
