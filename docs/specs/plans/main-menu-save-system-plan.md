# Main Menu & Save System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a black-screen main menu with New Game (variant picker) / Continue flow, backed by a JSON save system behind an interface abstraction, with a debug overlay to display saved data.

**Architecture:** Game code talks to `USaveDataSubsystem` (UGameInstanceSubsystem), which delegates to `ISaveDataProvider` (UInterface). The concrete implementation `UJsonFileSaveProvider` reads/writes raw JSON to disk. A `UMainMenuWidget` handles the menu flow, and `UGameDataDebugWidget` shows saved data as a debug overlay. The main menu lives in its own level with a dedicated game mode.

**Tech Stack:** Unreal Engine 5.7 C++, UMG widgets, FJsonSerializer, FFileHelper, UGameInstanceSubsystem, UInterface

**Spec:** `docs/specs/main-menu-save-system.md`

**Build/Test commands:**
- Compile: `task compile`
- Test: `task test`
- Both: `task check`

**Test conventions:**
- `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`
- Path: `"KillAllGit.<System>.<Component>.<TestName>"`
- Test files in `Source/KillAllGit/Tests/SaveSystem/`

---

## Task 1: Build System Setup

**Files:**
- Modify: `Source/KillAllGit/KillAllGit.Build.cs`

This task adds the new directories to the include paths and adds `Json` and `JsonUtilities` module dependencies so JSON parsing compiles.

- [ ] **Step 1: Add include paths and module dependencies**

In `Source/KillAllGit/KillAllGit.Build.cs`, add the new include paths and JSON modules:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",
    "AIModule",
    "StateTreeModule",
    "GameplayStateTreeModule",
    "UMG",
    "Slate",
    "Json",
    "JsonUtilities"
});
```

And add these to the `PublicIncludePaths` array:

```csharp
"KillAllGit/SaveSystem",
"KillAllGit/UI",
"KillAllGit/Tests/SaveSystem",
```

- [ ] **Step 2: Create directory structure**

Create these empty directories:
```
Source/KillAllGit/SaveSystem/
Source/KillAllGit/UI/
Source/KillAllGit/Tests/SaveSystem/
```

- [ ] **Step 3: Compile to verify no regressions**

Run: `task compile`
Expected: Success with no errors.

- [ ] **Step 4: Commit**

```bash
git add Source/KillAllGit/KillAllGit.Build.cs
git commit -m "[SaveSystem]: Add build dependencies and directory structure for save system and UI"
```

---

## Task 2: Save Data Types

**Files:**
- Create: `Source/KillAllGit/SaveSystem/SaveDataTypes.h`

Pure data types with no logic. These are the shared vocabulary for the save system.

- [ ] **Step 1: Create SaveDataTypes.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SaveDataTypes.generated.h"

UENUM(BlueprintType)
enum class EGameVariant : uint8
{
    Combat        UMETA(DisplayName = "Combat"),
    SideScrolling UMETA(DisplayName = "Side Scrolling"),
    Platforming   UMETA(DisplayName = "Platforming")
};

USTRUCT(BlueprintType)
struct FSaveDataPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EGameVariant Variant = EGameVariant::Combat;

    UPROPERTY(BlueprintReadOnly)
    FString GitHubDataJson;

    UPROPERTY(BlueprintReadOnly)
    FDateTime CreatedAt;
};

namespace GameVariantUtils
{
    inline FString GetMapPath(EGameVariant Variant)
    {
        switch (Variant)
        {
        case EGameVariant::Combat:
            return TEXT("/Game/Variant_Combat/Lvl_Combat");
        case EGameVariant::SideScrolling:
            return TEXT("/Game/Variant_SideScrolling/Lvl_SideScrolling");
        case EGameVariant::Platforming:
            return TEXT("/Game/Variant_Platforming/Lvl_Platforming");
        default:
            return TEXT("/Game/Variant_Combat/Lvl_Combat");
        }
    }

    inline FString VariantToString(EGameVariant Variant)
    {
        switch (Variant)
        {
        case EGameVariant::Combat:        return TEXT("Combat");
        case EGameVariant::SideScrolling: return TEXT("SideScrolling");
        case EGameVariant::Platforming:   return TEXT("Platforming");
        default:                          return TEXT("Combat");
        }
    }

    inline EGameVariant StringToVariant(const FString& Str)
    {
        if (Str == TEXT("SideScrolling")) return EGameVariant::SideScrolling;
        if (Str == TEXT("Platforming"))   return EGameVariant::Platforming;
        return EGameVariant::Combat;
    }
}
```

- [ ] **Step 2: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 3: Commit**

```bash
git add Source/KillAllGit/SaveSystem/SaveDataTypes.h
git commit -m "[SaveSystem]: Add EGameVariant enum and FSaveDataPayload struct"
```

---

## Task 3: Save Data Provider Interface

**Files:**
- Create: `Source/KillAllGit/SaveSystem/SaveDataProvider.h`

UInterface that all save implementations conform to. Game code never sees concrete types.

- [ ] **Step 1: Create SaveDataProvider.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveDataTypes.h"
#include "SaveDataProvider.generated.h"

UINTERFACE(MinimalAPI)
class USaveDataProvider : public UInterface
{
    GENERATED_BODY()
};

class ISaveDataProvider
{
    GENERATED_BODY()

public:
    virtual bool HasSaveData() const = 0;
    virtual FSaveDataPayload LoadSaveData() const = 0;
    virtual bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson) = 0;
    virtual bool DeleteSaveData() = 0;
};
```

- [ ] **Step 2: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 3: Commit**

```bash
git add Source/KillAllGit/SaveSystem/SaveDataProvider.h
git commit -m "[SaveSystem]: Add ISaveDataProvider interface"
```

---

## Task 4: Mock GitHub Data

**Files:**
- Create: `Source/KillAllGit/SaveSystem/MockGitHubData.h`

Static mock JSON blob. One place to find and replace when the real API lands.

- [ ] **Step 1: Create MockGitHubData.h**

```cpp
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
```

- [ ] **Step 2: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 3: Commit**

```bash
git add Source/KillAllGit/SaveSystem/MockGitHubData.h
git commit -m "[SaveSystem]: Add mock GitHub data for development"
```

---

## Task 5: JSON File Save Provider — Tests

**Files:**
- Create: `Source/KillAllGit/Tests/SaveSystem/JsonFileSaveProviderTest.cpp`

Write the tests before the implementation. These tests exercise file I/O in a temp directory.

- [ ] **Step 1: Write tests**

```cpp
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "SaveDataTypes.h"

// Forward declare — implementation doesn't exist yet, tests will fail to compile.
// We'll create a minimal header stub in step 2 so tests compile but fail.
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
```

- [ ] **Step 2: Create minimal header stub so tests compile**

Create `Source/KillAllGit/SaveSystem/JsonFileSaveProvider.h` with just enough to compile:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SaveDataProvider.h"
#include "JsonFileSaveProvider.generated.h"

UCLASS()
class UJsonFileSaveProvider : public UObject, public ISaveDataProvider
{
    GENERATED_BODY()

public:
    void SetSaveDirectory(const FString& InDirectory);

    virtual bool HasSaveData() const override;
    virtual FSaveDataPayload LoadSaveData() const override;
    virtual bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson) override;
    virtual bool DeleteSaveData() override;

private:
    FString SaveDirectory;
    FString GetSaveFilePath() const;
};
```

Create `Source/KillAllGit/SaveSystem/JsonFileSaveProvider.cpp` with stubs that fail:

```cpp
#include "JsonFileSaveProvider.h"

void UJsonFileSaveProvider::SetSaveDirectory(const FString& InDirectory)
{
    SaveDirectory = InDirectory;
}

FString UJsonFileSaveProvider::GetSaveFilePath() const
{
    return FString();
}

bool UJsonFileSaveProvider::HasSaveData() const
{
    return false;
}

FSaveDataPayload UJsonFileSaveProvider::LoadSaveData() const
{
    return FSaveDataPayload();
}

bool UJsonFileSaveProvider::CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson)
{
    return false;
}

bool UJsonFileSaveProvider::DeleteSaveData()
{
    return false;
}
```

- [ ] **Step 3: Compile and run tests**

Run: `task compile`
Expected: Success.

Run: `task test`
Expected: `HasSaveData_ReturnsFalseWhenNoFile` passes (stub returns false). Other 3 tests FAIL.

- [ ] **Step 4: Commit**

```bash
git add Source/KillAllGit/Tests/SaveSystem/JsonFileSaveProviderTest.cpp \
       Source/KillAllGit/SaveSystem/JsonFileSaveProvider.h \
       Source/KillAllGit/SaveSystem/JsonFileSaveProvider.cpp
git commit -m "[SaveSystem]: Add JsonFileSaveProvider tests and stub implementation"
```

---

## Task 6: JSON File Save Provider — Implementation

**Files:**
- Modify: `Source/KillAllGit/SaveSystem/JsonFileSaveProvider.cpp`

Implement the real file I/O to make all tests pass.

- [ ] **Step 1: Implement JsonFileSaveProvider.cpp**

Replace the full contents of `Source/KillAllGit/SaveSystem/JsonFileSaveProvider.cpp`:

```cpp
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
        FDateTime::Iso8601Parse(CreatedAtString, Payload.CreatedAt);
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
```

- [ ] **Step 2: Run tests**

Run: `task test`
Expected: All 4 `KillAllGit.SaveSystem.JsonFileSaveProvider.*` tests PASS.

- [ ] **Step 3: Commit**

```bash
git add Source/KillAllGit/SaveSystem/JsonFileSaveProvider.cpp
git commit -m "[SaveSystem]: Implement JsonFileSaveProvider with file I/O"
```

---

## Task 7: Save Data Subsystem

**Files:**
- Create: `Source/KillAllGit/SaveSystem/SaveDataSubsystem.h`
- Create: `Source/KillAllGit/SaveSystem/SaveDataSubsystem.cpp`
- Create: `Source/KillAllGit/Tests/SaveSystem/SaveDataSubsystemTest.cpp`

The subsystem is the composition root — the only place that knows about `UJsonFileSaveProvider`. Game code uses the subsystem's public API.

- [ ] **Step 1: Write the subsystem test**

Create `Source/KillAllGit/Tests/SaveSystem/SaveDataSubsystemTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "SaveDataSubsystem.h"
#include "SaveDataTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveDataSubsystem_InitialState,
    "KillAllGit.SaveSystem.SaveDataSubsystem.InitialStateHasNoSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FSaveDataSubsystem_InitialState::RunTest(const FString& Parameters)
{
    USaveDataSubsystem* Subsystem = NewObject<USaveDataSubsystem>();
    Subsystem->InitializeForTest(FPaths::ProjectSavedDir() / TEXT("TestSubsystemData"));

    TestFalse("Should have no save data initially", Subsystem->HasSaveData());

    Subsystem->DeleteSaveData();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSaveDataSubsystem_CreateAndLoad,
    "KillAllGit.SaveSystem.SaveDataSubsystem.CreateAndLoadRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)
bool FSaveDataSubsystem_CreateAndLoad::RunTest(const FString& Parameters)
{
    USaveDataSubsystem* Subsystem = NewObject<USaveDataSubsystem>();
    Subsystem->InitializeForTest(FPaths::ProjectSavedDir() / TEXT("TestSubsystemData"));

    const FString MockJson = TEXT(R"({"name":"test-repo"})");
    const bool Created = Subsystem->CreateSaveData(EGameVariant::Platforming, MockJson);
    TestTrue("CreateSaveData should succeed", Created);

    const FSaveDataPayload Payload = Subsystem->LoadSaveData();
    TestEqual("Variant should match", Payload.Variant, EGameVariant::Platforming);
    TestTrue("JSON should contain repo name", Payload.GitHubDataJson.Contains(TEXT("test-repo")));

    Subsystem->DeleteSaveData();
    return true;
}
```

- [ ] **Step 2: Create SaveDataSubsystem.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveDataTypes.h"
#include "SaveDataSubsystem.generated.h"

class UJsonFileSaveProvider;

UCLASS()
class USaveDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void InitializeForTest(const FString& TestSaveDirectory);

    UFUNCTION(BlueprintCallable, Category = "Save Data")
    bool HasSaveData() const;

    UFUNCTION(BlueprintCallable, Category = "Save Data")
    FSaveDataPayload LoadSaveData() const;

    UFUNCTION(BlueprintCallable, Category = "Save Data")
    bool CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson);

    UFUNCTION(BlueprintCallable, Category = "Save Data")
    bool DeleteSaveData();

    UFUNCTION(BlueprintCallable, Category = "Save Data")
    FSaveDataPayload GetCurrentSaveData() const;

private:
    UPROPERTY()
    TObjectPtr<UJsonFileSaveProvider> Provider;

    FSaveDataPayload CachedPayload;
    bool bHasCachedPayload = false;

    void CreateProvider(const FString& SaveDirectory = FString());
};
```

- [ ] **Step 3: Create SaveDataSubsystem.cpp**

```cpp
#include "SaveDataSubsystem.h"
#include "JsonFileSaveProvider.h"

void USaveDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CreateProvider();
}

void USaveDataSubsystem::InitializeForTest(const FString& TestSaveDirectory)
{
    CreateProvider(TestSaveDirectory);
}

void USaveDataSubsystem::CreateProvider(const FString& SaveDirectory)
{
    Provider = NewObject<UJsonFileSaveProvider>(this);
    if (!SaveDirectory.IsEmpty())
    {
        Provider->SetSaveDirectory(SaveDirectory);
    }
    bHasCachedPayload = false;
}

bool USaveDataSubsystem::HasSaveData() const
{
    return Provider ? Provider->HasSaveData() : false;
}

FSaveDataPayload USaveDataSubsystem::LoadSaveData() const
{
    if (!Provider)
    {
        return FSaveDataPayload();
    }

    FSaveDataPayload Payload = Provider->LoadSaveData();

    // Cache it (mutable because this is a read-through cache)
    USaveDataSubsystem* MutableThis = const_cast<USaveDataSubsystem*>(this);
    MutableThis->CachedPayload = Payload;
    MutableThis->bHasCachedPayload = true;

    return Payload;
}

bool USaveDataSubsystem::CreateSaveData(EGameVariant Variant, const FString& GitHubDataJson)
{
    if (!Provider)
    {
        return false;
    }

    const bool Result = Provider->CreateSaveData(Variant, GitHubDataJson);
    if (Result)
    {
        CachedPayload = Provider->LoadSaveData();
        bHasCachedPayload = true;
    }
    return Result;
}

bool USaveDataSubsystem::DeleteSaveData()
{
    if (!Provider)
    {
        return false;
    }

    const bool Result = Provider->DeleteSaveData();
    if (Result)
    {
        CachedPayload = FSaveDataPayload();
        bHasCachedPayload = false;
    }
    return Result;
}

FSaveDataPayload USaveDataSubsystem::GetCurrentSaveData() const
{
    if (bHasCachedPayload)
    {
        return CachedPayload;
    }
    return LoadSaveData();
}
```

- [ ] **Step 4: Compile and run tests**

Run: `task compile`
Expected: Success.

Run: `task test`
Expected: All `KillAllGit.SaveSystem.*` tests PASS (6 total: 4 provider + 2 subsystem).

- [ ] **Step 5: Commit**

```bash
git add Source/KillAllGit/SaveSystem/SaveDataSubsystem.h \
       Source/KillAllGit/SaveSystem/SaveDataSubsystem.cpp \
       Source/KillAllGit/Tests/SaveSystem/SaveDataSubsystemTest.cpp
git commit -m "[SaveSystem]: Add SaveDataSubsystem with cached provider delegation"
```

---

## Task 8: Main Menu Game Mode

**Files:**
- Create: `Source/KillAllGit/UI/MainMenuGameMode.h`
- Create: `Source/KillAllGit/UI/MainMenuGameMode.cpp`

Minimal game mode that creates the main menu widget and sets input to UI-only. The widget class is set via Blueprint (UPROPERTY EditAnywhere) so we can iterate on widget layout without recompiling.

- [ ] **Step 1: Create MainMenuGameMode.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

class UUserWidget;

UCLASS(abstract)
class AMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> MainMenuWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> CurrentWidget;
};
```

- [ ] **Step 2: Create MainMenuGameMode.cpp**

```cpp
#include "MainMenuGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

void AMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!MainMenuWidgetClass)
    {
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        return;
    }

    CurrentWidget = CreateWidget<UUserWidget>(PC, MainMenuWidgetClass);
    if (CurrentWidget)
    {
        CurrentWidget->AddToViewport();
    }

    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;
}
```

- [ ] **Step 3: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 4: Commit**

```bash
git add Source/KillAllGit/UI/MainMenuGameMode.h \
       Source/KillAllGit/UI/MainMenuGameMode.cpp
git commit -m "[UI]: Add MainMenuGameMode with widget creation and UI input mode"
```

---

## Task 9: Main Menu Widget

**Files:**
- Create: `Source/KillAllGit/UI/MainMenuWidget.h`
- Create: `Source/KillAllGit/UI/MainMenuWidget.cpp`

C++ base class for the main menu. Handles the logic (button visibility, save check, level loading). Layout will be done in a Blueprint subclass, but the C++ class creates buttons programmatically via Slate/UMG for now since we want a simple black screen with minimal Blueprint work.

- [ ] **Step 1: Create MainMenuWidget.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveDataTypes.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class USaveDataSubsystem;

UCLASS()
class UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY()
    TObjectPtr<UButton> NewGameButton;

    UPROPERTY()
    TObjectPtr<UButton> ContinueButton;

    UPROPERTY()
    TObjectPtr<UVerticalBox> VariantPickerBox;

    UFUNCTION()
    void OnNewGameClicked();

    UFUNCTION()
    void OnContinueClicked();

    UFUNCTION()
    void OnSelectCombat();

    UFUNCTION()
    void OnSelectSideScrolling();

    UFUNCTION()
    void OnSelectPlatforming();

    void OnVariantSelected(EGameVariant Variant);
    void ShowMainMenu();
    void ShowVariantPicker();
    void StartGame(EGameVariant Variant, bool bIsNewGame);

    USaveDataSubsystem* GetSaveSubsystem() const;
};
```

- [ ] **Step 2: Create MainMenuWidget.cpp**

```cpp
#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "SaveDataSubsystem.h"
#include "MockGitHubData.h"
#include "Kismet/GameplayStatics.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ShowMainMenu();
}

USaveDataSubsystem* UMainMenuWidget::GetSaveSubsystem() const
{
    const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
    return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UMainMenuWidget::ShowMainMenu()
{
    if (UPanelWidget* RootPanel = Cast<UPanelWidget>(GetRootWidget()))
    {
        RootPanel->ClearChildren();
    }

    UVerticalBox* VBox = NewObject<UVerticalBox>(this);

    NewGameButton = NewObject<UButton>(this);
    UTextBlock* NewGameText = NewObject<UTextBlock>(this);
    NewGameText->SetText(FText::FromString(TEXT("New Game")));
    NewGameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    NewGameButton->AddChild(NewGameText);
    NewGameButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnNewGameClicked);
    VBox->AddChild(NewGameButton);

    const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
    if (SaveSubsystem && SaveSubsystem->HasSaveData())
    {
        ContinueButton = NewObject<UButton>(this);
        UTextBlock* ContinueText = NewObject<UTextBlock>(this);
        ContinueText->SetText(FText::FromString(TEXT("Continue")));
        ContinueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        ContinueButton->AddChild(ContinueText);
        ContinueButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnContinueClicked);
        VBox->AddChild(ContinueButton);
    }

    SetRootWidget(VBox);
}

void UMainMenuWidget::ShowVariantPicker()
{
    if (UPanelWidget* RootPanel = Cast<UPanelWidget>(GetRootWidget()))
    {
        RootPanel->ClearChildren();
    }

    VariantPickerBox = NewObject<UVerticalBox>(this);

    UTextBlock* Title = NewObject<UTextBlock>(this);
    Title->SetText(FText::FromString(TEXT("Select Variant")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    VariantPickerBox->AddChild(Title);

    auto AddVariantButton = [this](const FString& Label, FName HandlerName)
    {
        UButton* Btn = NewObject<UButton>(this);
        UTextBlock* BtnText = NewObject<UTextBlock>(this);
        BtnText->SetText(FText::FromString(Label));
        BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Btn->AddChild(BtnText);

        FScriptDelegate Delegate;
        Delegate.BindUFunction(this, HandlerName);
        Btn->OnClicked.Add(Delegate);

        VariantPickerBox->AddChild(Btn);
    };

    AddVariantButton(TEXT("Combat"), GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectCombat));
    AddVariantButton(TEXT("Side Scrolling"), GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectSideScrolling));
    AddVariantButton(TEXT("Platforming"), GET_FUNCTION_NAME_CHECKED(UMainMenuWidget, OnSelectPlatforming));

    SetRootWidget(VariantPickerBox);
}

void UMainMenuWidget::OnNewGameClicked()
{
    ShowVariantPicker();
}

void UMainMenuWidget::OnContinueClicked()
{
    const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
    if (!SaveSubsystem)
    {
        return;
    }

    const FSaveDataPayload Payload = SaveSubsystem->LoadSaveData();
    StartGame(Payload.Variant, false);
}

void UMainMenuWidget::OnSelectCombat()
{
    OnVariantSelected(EGameVariant::Combat);
}

void UMainMenuWidget::OnSelectSideScrolling()
{
    OnVariantSelected(EGameVariant::SideScrolling);
}

void UMainMenuWidget::OnSelectPlatforming()
{
    OnVariantSelected(EGameVariant::Platforming);
}

void UMainMenuWidget::OnVariantSelected(EGameVariant Variant)
{
    StartGame(Variant, true);
}

void UMainMenuWidget::StartGame(EGameVariant Variant, bool bIsNewGame)
{
    if (bIsNewGame)
    {
        USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
        if (SaveSubsystem)
        {
            SaveSubsystem->CreateSaveData(Variant, FMockGitHubData::GetMockJson());
        }
    }

    const FString MapPath = GameVariantUtils::GetMapPath(Variant);
    UGameplayStatics::OpenLevel(GetWorld(), FName(*MapPath));
}
```

- [ ] **Step 3: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 5: Commit**

```bash
git add Source/KillAllGit/UI/MainMenuWidget.h \
       Source/KillAllGit/UI/MainMenuWidget.cpp
git commit -m "[UI]: Add MainMenuWidget with New Game variant picker and Continue flow"
```

---

## Task 10: Game Data Debug Widget

**Files:**
- Create: `Source/KillAllGit/UI/GameDataDebugWidget.h`
- Create: `Source/KillAllGit/UI/GameDataDebugWidget.cpp`

Debug overlay that reads from the subsystem and displays the raw data as text.

- [ ] **Step 1: Create GameDataDebugWidget.h**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameDataDebugWidget.generated.h"

class UTextBlock;
class USaveDataSubsystem;

UCLASS()
class UGameDataDebugWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY()
    TObjectPtr<UTextBlock> DebugTextBlock;

    void RefreshDisplay();
    USaveDataSubsystem* GetSaveSubsystem() const;
};
```

- [ ] **Step 2: Create GameDataDebugWidget.cpp**

```cpp
#include "GameDataDebugWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "SaveDataSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UGameDataDebugWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UVerticalBox* VBox = NewObject<UVerticalBox>(this);

    UTextBlock* Header = NewObject<UTextBlock>(this);
    Header->SetText(FText::FromString(TEXT("=== Game Data Debug ===")));
    Header->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
    VBox->AddChild(Header);

    DebugTextBlock = NewObject<UTextBlock>(this);
    DebugTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
    VBox->AddChild(DebugTextBlock);

    SetRootWidget(VBox);
    RefreshDisplay();
}

void UGameDataDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshDisplay();
}

USaveDataSubsystem* UGameDataDebugWidget::GetSaveSubsystem() const
{
    const UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
    return GI ? GI->GetSubsystem<USaveDataSubsystem>() : nullptr;
}

void UGameDataDebugWidget::RefreshDisplay()
{
    if (!DebugTextBlock)
    {
        return;
    }

    const USaveDataSubsystem* SaveSubsystem = GetSaveSubsystem();
    if (!SaveSubsystem || !SaveSubsystem->HasSaveData())
    {
        DebugTextBlock->SetText(FText::FromString(TEXT("No save data")));
        return;
    }

    const FSaveDataPayload Payload = SaveSubsystem->GetCurrentSaveData();

    FString Display = FString::Printf(
        TEXT("Variant: %s\nCreated: %s\n\nGitHub Data:\n%s"),
        *GameVariantUtils::VariantToString(Payload.Variant),
        *Payload.CreatedAt.ToString(),
        *Payload.GitHubDataJson
    );

    DebugTextBlock->SetText(FText::FromString(Display));
}
```

- [ ] **Step 3: Compile**

Run: `task compile`
Expected: Success.

- [ ] **Step 4: Commit**

```bash
git add Source/KillAllGit/UI/GameDataDebugWidget.h \
       Source/KillAllGit/UI/GameDataDebugWidget.cpp
git commit -m "[UI]: Add GameDataDebugWidget for displaying save data overlay"
```

---

## Task 11: Level and Config Setup

**Files:**
- Modify: `Config/DefaultEngine.ini`

Set the main menu as the default startup map. The actual `Lvl_MainMenu` map and Blueprint subclasses (BP_MainMenuGameMode, WBP_MainMenu, WBP_GameDataDebug) need to be created in the Unreal Editor — they can't be created from C++ or text files.

- [ ] **Step 1: Update DefaultEngine.ini**

Change the `GameDefaultMap` and `EditorStartupMap` lines:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/MainMenu/Lvl_MainMenu.Lvl_MainMenu
EditorStartupMap=/Game/MainMenu/Lvl_MainMenu.Lvl_MainMenu
GlobalDefaultGameMode=/Game/MainMenu/Blueprints/BP_MainMenuGameMode.BP_MainMenuGameMode_C
```

- [ ] **Step 2: Document the manual editor steps**

These must be done manually in the Unreal Editor after the C++ code compiles:

1. Create folder `Content/MainMenu/Blueprints/`
2. Create new level: `Content/MainMenu/Lvl_MainMenu` — empty level with black background (no sky, no lights)
3. Create Blueprint class `BP_MainMenuGameMode` (parent: `AMainMenuGameMode`)
   - Set `MainMenuWidgetClass` to `WBP_MainMenu`
4. Create Widget Blueprint `WBP_MainMenu` (parent: `UMainMenuWidget`)
   - No designer edits needed — C++ builds the UI programmatically
5. Create Widget Blueprint `WBP_GameDataDebug` (parent: `UGameDataDebugWidget`)
   - No designer edits needed — C++ builds the debug overlay programmatically
6. In `Lvl_MainMenu` world settings, set Game Mode Override to `BP_MainMenuGameMode`

- [ ] **Step 3: Compile and run all tests**

Run: `task check`
Expected: Compile succeeds. All tests pass (6 SaveSystem tests + 1 SmokeTest = 7 total).

- [ ] **Step 4: Commit**

```bash
git add Config/DefaultEngine.ini
git commit -m "[UI]: Set main menu as default startup level"
```

---

## Task 12: Final Verification

- [ ] **Step 1: Run full check**

Run: `task check`
Expected: Compile succeeds. All tests pass.

- [ ] **Step 2: Manual play test**

In the Unreal Editor (after completing Task 11's manual steps):
1. PIE (Play In Editor) — should see black screen with "New Game" button
2. Click "New Game" → should see 3 variant buttons
3. Click "Combat" → should load Combat level
4. Stop, re-PIE → should now see "Continue" button alongside "New Game"
5. Click "Continue" → should load Combat level (the one saved)

- [ ] **Step 3: Update spec status**

In `docs/specs/main-menu-save-system.md`, check off completed items:

```markdown
## Status

- [x] ISaveDataProvider
- [x] FSaveDataPayload / EGameVariant
- [x] UJsonFileSaveProvider
- [x] USaveDataSubsystem
- [x] FMockGitHubData
- [x] UMainMenuWidget
- [x] AMainMenuGameMode
- [x] UGameDataDebugWidget
- [ ] Lvl_MainMenu (requires manual editor work)
- [x] Config: set default map
- [x] Tests
```

- [ ] **Step 4: Final commit**

```bash
git add docs/specs/main-menu-save-system.md
git commit -m "[SaveSystem]: Mark save system and UI implementation complete in spec"
```
