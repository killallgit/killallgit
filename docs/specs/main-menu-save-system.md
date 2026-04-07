# Main Menu & Save System

Black-screen main menu with New Game / Continue flow, variant picker, and a JSON-backed save system behind an interface abstraction.

## Components

### ISaveDataProvider (UInterface)

Pure interface for save data access. All game code depends on this — never on the concrete implementation.

- `HasSaveData()` → bool
- `LoadSaveData()` → `FSaveDataPayload`
- `CreateSaveData(EGameVariant, JsonString)` → bool
- `DeleteSaveData()` → bool

### FSaveDataPayload (UStruct)

Parsed save data returned by the provider:

- `Variant` (EGameVariant enum: Combat, SideScrolling, Platforming)
- `GitHubDataJson` (FString — raw JSON blob)
- `CreatedAt` (FDateTime)

### EGameVariant (UENUM)

Enum mapping variant names to their level paths:

- `Combat` → `/Game/Variant_Combat/Lvl_Combat`
- `SideScrolling` → `/Game/Variant_SideScrolling/Lvl_SideScrolling`
- `Platforming` → `/Game/Variant_Platforming/Lvl_Platforming`

### UJsonFileSaveProvider (UObject, implements ISaveDataProvider)

Concrete implementation. Reads/writes a single JSON file at `Saved/GameData/save.json`.

File format:

```json
{
  "variant": "Combat",
  "createdAt": "2026-04-07T12:00:00Z",
  "githubData": {
    "name": "vscode",
    "description": "Visual Studio Code",
    "stargazerCount": 170000,
    "forkCount": 30000,
    "primaryLanguage": "TypeScript"
  }
}
```

- Uses `FFileHelper` for file I/O and `FJsonSerializer` for parsing
- Save path resolved via `FPaths::ProjectSavedDir() / TEXT("GameData/save.json")`
- No network knowledge — receives JSON as a string

### USaveDataSubsystem (UGameInstanceSubsystem)

Orchestrator. Owns the `ISaveDataProvider` instance.

- `Initialize()` creates `UJsonFileSaveProvider` (composition root — only place that knows the concrete type)
- Exposes the same interface methods, delegating to the provider
- Game systems access save data through this subsystem only
- Holds the current `FSaveDataPayload` in memory after load

### UMainMenuWidget (UUserWidget)

Full-screen black widget with centered buttons.

- "New Game" button — always visible, opens variant picker
- "Continue" button — visible only when `USaveDataSubsystem::HasSaveData()` returns true
- Variant picker: 3 buttons (Combat, SideScrolling, Platforming), shown after pressing New Game
- On variant select: generates mock GitHub JSON → calls `CreateSaveData(variant, mockJson)` → `UGameplayStatics::OpenLevel()` with the variant's map path
- On continue: calls `LoadSaveData()` → opens the level matching the saved variant

### AMainMenuGameMode (AGameModeBase)

Minimal game mode for the main menu level.

- `BeginPlay()` creates and displays `UMainMenuWidget`
- Sets input mode to UI-only
- Shows mouse cursor

### Debug Display

`USaveDataSubsystem::ShowSaveData()` — exec console command. Type `ShowSaveData` in the console to print current save data to the screen for 5 seconds via `GEngine->AddOnScreenDebugMessage()`.

**Backlog:** Replace with a custom Gameplay Debugger category (`FGameplayDebuggerCategory`) for a toggleable, persistent debug overlay that integrates with UE's built-in debug system (`'` key).

### Mock Data

Hardcoded mock JSON string used by New Game until the GitHub API layer is connected:

```json
{
  "name": "vscode",
  "description": "Visual Studio Code",
  "owner": "microsoft",
  "stargazerCount": 170000,
  "forkCount": 30000,
  "primaryLanguage": "TypeScript",
  "createdAt": "2015-09-03T20:23:38Z",
  "defaultBranchName": "main"
}
```

Lives as a static function on a `FMockGitHubData` struct — easy to find and replace later.

## Data Flow

```
Main Menu
  → "New Game"
      → Variant Picker (Combat / SideScrolling / Platforming)
      → User picks variant
      → FMockGitHubData::GetMockJson()
      → USaveDataSubsystem::CreateSaveData(variant, mockJson)
          → UJsonFileSaveProvider writes Saved/GameData/save.json
      → UGameplayStatics::OpenLevel(variant map path)

  → "Continue"
      → USaveDataSubsystem::LoadSaveData()
          → UJsonFileSaveProvider reads Saved/GameData/save.json
          → Parses into FSaveDataPayload
      → UGameplayStatics::OpenLevel(payload.Variant map path)

In-Game:
  → Console command "ShowSaveData"
      → USaveDataSubsystem::ShowSaveData()
      → Prints variant + GitHub JSON fields to screen via GEngine
```

## Level Setup

- New map: `Content/MainMenu/Lvl_MainMenu.umap`
- Set as the default/entry map in `Config/DefaultEngine.ini`
- Uses `AMainMenuGameMode` (set in world settings or via GameMode override)

## File Map

| Class | File |
|-------|------|
| `ISaveDataProvider` | `Source/KillAllGit/SaveSystem/SaveDataProvider.h` |
| `FSaveDataPayload` | `Source/KillAllGit/SaveSystem/SaveDataTypes.h` |
| `EGameVariant` | `Source/KillAllGit/SaveSystem/SaveDataTypes.h` |
| `UJsonFileSaveProvider` | `Source/KillAllGit/SaveSystem/JsonFileSaveProvider.h/.cpp` |
| `USaveDataSubsystem` | `Source/KillAllGit/SaveSystem/SaveDataSubsystem.h/.cpp` |
| `FMockGitHubData` | `Source/KillAllGit/SaveSystem/MockGitHubData.h` |
| `UMainMenuWidget` | `Source/KillAllGit/UI/MainMenuWidget.h/.cpp` |
| `AMainMenuGameMode` | `Source/KillAllGit/UI/MainMenuGameMode.h/.cpp` |
| Main menu level | `Content/MainMenu/Lvl_MainMenu.umap` |
| Save data tests | `Source/KillAllGit/Tests/SaveSystem/JsonFileSaveProviderTest.cpp` |
| Subsystem tests | `Source/KillAllGit/Tests/SaveSystem/SaveDataSubsystemTest.cpp` |

## Decisions Log

- **2026-04-07:** JSON file for save data rather than USaveGame. Keeps things simple and debuggable while the save format is evolving. Interface abstraction means swapping to USaveGame later doesn't touch game code.
- **2026-04-07:** ISaveDataProvider interface rather than coding directly to UJsonFileSaveProvider. Follows the project's DI pattern and allows testing with mock providers.
- **2026-04-07:** Mock GitHub data as a static struct rather than a test fixture file. Stays in-code for discoverability, easy to grep and replace when real API layer lands.
- **2026-04-07:** Single save file rather than per-slot saves. No need for multiple save slots yet — YAGNI.
- **2026-04-07:** EGameVariant enum with map path mapping rather than string-based variant names. Type-safe, compiler-checked, no typo risk.

## Status

- [x] ISaveDataProvider
- [x] FSaveDataPayload / EGameVariant
- [x] UJsonFileSaveProvider
- [x] USaveDataSubsystem
- [x] FMockGitHubData
- [x] UMainMenuWidget
- [x] AMainMenuGameMode
- [x] Debug display (ShowSaveData exec command)
- [ ] Lvl_MainMenu (requires manual editor work)
- [x] Config: set default map
- [x] Tests
