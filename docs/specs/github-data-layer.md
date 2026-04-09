# GitHub Data Layer

Fetches GitHub repository data via GraphQL, caches responses to disk as JSON, and serves parsed data to game systems.

## Components

### UGitHubAPIClient (UObject)

HTTP client for the GitHub GraphQL API.

- POSTs to `https://api.github.com/graphql`
- Auth token obtained by shelling out to `gh auth token`
- Loads `.graphql` query files from `Content/Queries/`, injects variables
- Returns raw JSON string via async delegate

### UGitHubDataCache (UObject)

File-based JSON cache in `Saved/GitHubCache/`.

- Key = sanitized repo identifier (e.g. `microsoft_vscode.json`)
- `HasCache(RepoId)` / `ReadCache(RepoId)` / `WriteCache(RepoId, JsonString)`
- Pure file I/O — no network knowledge

### UGitHubDataSubsystem (UGameInstanceSubsystem)

Orchestrator. Owns the API client and cache.

- `RequestRepositoryData(Owner, Name)` → cache check → fetch if miss → cache on success → fire delegate
- Holds parsed result as `FGitHubRepositoryData`

### FGitHubRepositoryData (UStruct)

Mapped from GraphQL response:

- Name, Description, URL
- StargazerCount, ForkCount
- CreatedAt, UpdatedAt
- PrimaryLanguage
- DefaultBranchName

### Debug Overlay (UGitHubDebugWidget — UUserWidget)

Displays raw repository data fields as text. Toggled via console command or debug key.

## Query Files

Stored as `.graphql` text files in `Content/Queries/`.

- `GetRepository.graphql` — fetches repository metadata with `$owner` and `$name` variables

## Data Flow

```
RequestRepositoryData("microsoft", "vscode")
  → UGitHubDataCache::HasCache("microsoft_vscode")
  → HIT:  ReadCache → parse JSON → FGitHubRepositoryData → delegate
  → MISS: Load GetRepository.graphql
          → gh auth token (shell)
          → POST to GitHub GraphQL API
          → WriteCache("microsoft_vscode", response)
          → parse JSON → FGitHubRepositoryData → delegate
```

## File Map

| Class | File |
|-------|------|
| `UGitHubAPIClient` | `Source/KillAllGit/GitHub/GitHubAPIClient.h/.cpp` |
| `UGitHubDataCache` | `Source/KillAllGit/GitHub/GitHubDataCache.h/.cpp` |
| `UGitHubDataSubsystem` | `Source/KillAllGit/GitHub/GitHubDataSubsystem.h/.cpp` |
| `FGitHubRepositoryData` | `Source/KillAllGit/GitHub/GitHubTypes.h` |
| `UGitHubDebugWidget` | `Source/KillAllGit/UI/GitHubDebugWidget.h/.cpp` |
| Repository query | `Content/Queries/GetRepository.graphql` |
| API client tests | `Source/KillAllGit/Tests/GitHub/GitHubAPIClientTest.cpp` |
| Cache tests | `Source/KillAllGit/Tests/GitHub/GitHubDataCacheTest.cpp` |
| Subsystem tests | `Source/KillAllGit/Tests/GitHub/GitHubDataSubsystemTest.cpp` |

## Decisions Log

- **2026-04-09:** Auth via `FPlatformMisc::GetEnvironmentVariable(TEXT("GITHUB_TOKEN"))` with fallback to reading `<ProjectDir>/.env` (gitignored, `KEY=VALUE` format). Shell env takes priority. `.env` fallback ensures editor launched outside terminal (Finder, Xcode) still works. If neither has the token, request is aborted with a logged error.
- **2026-04-07:** Raw JSON cache on disk rather than USaveGame for API responses. GitHub responses are deeply nested JSON — forcing through UPROPERTY serialization adds boilerplate for data we're just caching. USaveGame reserved for player/game state.
- **2026-04-07:** GraphQL queries as external `.graphql` files rather than hardcoded C++ strings. Allows iteration without recompilation, testable in GitHub's GraphQL Explorer.
- **2026-04-07:** UGameInstanceSubsystem for the manager rather than singleton or static. Standard UE5 pattern, no global state, engine-managed lifecycle.
- **2026-04-09:** `RequestRepositoryData` takes a `bForceRefresh` bool. When true, skips cache read but still writes to cache on success. Used by the main menu Refresh button so every press hits the API.
- **2026-04-09:** Debug output via `UE_LOG(LogKillAllGit, Display, ...)` — one line per field, `key: value` format. No widget or on-screen display for this milestone. Gameplay Debugger category deferred until in-game context exists.

## Status

- [ ] UGitHubAPIClient
- [ ] UGitHubDataCache
- [ ] UGitHubDebugWidget
- [ ] UGitHubDataSubsystem
- [ ] FGitHubRepositoryData
- [ ] GetRepository.graphql
- [ ] Tests
