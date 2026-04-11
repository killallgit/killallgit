# Game Initialization

Step-based initialization pipeline that runs on new game start. Downloads the target GitHub repo as a zip, computes a SHA256 fingerprint, and caches metadata to avoid redundant downloads. Reports progress to a main menu overlay.

## Data Model

### FRepoRecord (UStruct)

Single source of truth for all data about a repository. Replaces `FGitHubRepositoryData`. Stored as one JSON file per repo — flat fields only, no nesting. Designed to map directly to a SQLite row when the time comes.

Fields:

- Owner, Name — identity
- Description, Url, StargazerCount, ForkCount, CreatedAt, UpdatedAt, PrimaryLanguage, DefaultBranchName — from GitHub API
- ZipSha256 — SHA256 of the downloaded zip file
- ZipSizeBytes — size of the zip in bytes
- DownloadedAt — ISO 8601 timestamp of when the zip was fetched

Serialized via `FJsonObjectConverter` to `Saved/RepoData/{owner}_{repo}.json`.

### Migration from FGitHubRepositoryData

`FGitHubRepositoryData` is replaced by `FRepoRecord`. All consumers switch to the new type. The API metadata fields are identical — `FRepoRecord` adds identity (Owner, Name) and zip metadata.

`FSaveDataPayload::GitHubDataJson` (raw JSON string copy) is replaced with `RepoOwner` + `RepoName` — a reference to the `FRepoRecord` on disk. The save file no longer duplicates repo data.

## Components

### UGameInitPipeline (UObject)

Sequential async step runner. Each step has a name and reports fractional progress (0.0–1.0). The pipeline broadcasts three delegates:

- Progress — current step label + fraction
- Complete — all steps finished (or short-circuited via `Complete()`)
- Failed — pipeline aborted with a reason string (via `Fail(Reason)`)

Steps are enqueued and run one at a time. Each step receives a completion callback. `Complete()` short-circuits remaining steps (cache hit). `Fail(Reason)` aborts the pipeline (download error, missing token). Adding future steps (extraction, terrain gen) means adding one function — nothing else changes.

### UGameInitSubsystem (UGameInstanceSubsystem)

Owns the `UGameInitPipeline`. Entry point for triggering initialization from the main menu. Depends on `UGitHubDataSubsystem` (via `InitializeDependency`) and accesses repo records through its facade methods — never touches the cache directly.

### Initialization Steps (Milestone 1)

| Step | Label | What it does |
|------|-------|-------------|
| 1 | Checking cache... | Check via `UGitHubDataSubsystem::HasRepoZipData`. If true, skip to complete. |
| 2 | Downloading repository... | GET `https://api.github.com/repos/{owner}/{repo}/zipball/HEAD`. Reports Content-Length progress. Saves to temp file. |
| 3 | Verifying... | SHA256 the downloaded zip. Populate `ZipSha256` and `ZipSizeBytes` on the `FRepoRecord`. |
| 4 | Saving metadata... | Read-modify-write via `UGitHubDataSubsystem::UpdateRepoRecord` — merges zip fields onto existing record. Zip stays on disk for post-processing. |

On cache hit (step 1 finds existing record with SHA), steps 2–4 are skipped. The overlay still shows briefly — step 1 completes at 100%.

### UInitOverlayWidget (UUserWidget, abstract)

Overlay on the main menu. Created dynamically by `UMainMenuWidget` and added to viewport at Z-order 1 (above the menu). Blueprint subclass provides layout. Binds to the pipeline's progress, complete, and failed delegates. Hides on complete or failure.

```
[████████████░░░░░░]
Downloading repository...  48%
```

### UGitHubDataCache Evolution

Interface changes from raw string I/O to structured record I/O:

- `HasRecord(Owner, Name)` → bool
- `ReadRecord(Owner, Name)` → FRepoRecord
- `WriteRecord(Owner, Name, FRepoRecord)` → void

Storage moves from `Saved/GitHubCache/` to `Saved/RepoData/`. Uses `FJsonObjectConverter` for struct ↔ JSON.

## Data Flow

```
Main Menu → "New Game" → user enters repo → clicks variant button
  → UGameInitSubsystem::BeginInit(Owner, Name)
  → Show UInitOverlayWidget
  → Step 1: Check Saved/RepoData/{owner}_{repo}.json
      → HIT (ZipSha256 present): skip to complete
      → MISS: continue
  → Step 2: Download zipball from GitHub API
      → Progress via Content-Length
      → Save to Saved/RepoData/{owner}_{repo}.zip (temp)
  → Step 3: SHA256 the zip, record size
  → Step 4: Write FRepoRecord JSON (zip stays on disk for post-processing)
  → OnComplete → hide overlay → print zip size to console → open level
```

## File Map

| Class | File |
|-------|------|
| `FRepoRecord` | `Source/KillAllGit/GitHub/RepoRecord.h` |
| `UGitHubDataCache` | `Source/KillAllGit/GitHub/GitHubDataCache.h/.cpp` (modified) |
| `UGameInitPipeline` | `Source/KillAllGit/GameInit/GameInitPipeline.h/.cpp` |
| `UGameInitSubsystem` | `Source/KillAllGit/GameInit/GameInitSubsystem.h/.cpp` |
| `UInitOverlayWidget` | `Source/KillAllGit/UI/InitOverlayWidget.h/.cpp` |
| Pipeline tests | `Source/KillAllGit/Tests/GameInit/GameInitPipelineTest.cpp` |
| Cache tests | `Source/KillAllGit/Tests/GitHub/GitHubDataCacheTest.cpp` (modified) |

## Decisions Log

- **2026-04-10:** `FRepoRecord` as single source of truth replacing `FGitHubRepositoryData`. All repo data — API metadata, zip metadata, future file analysis — lives in one flat struct, one JSON file. Designed for trivial SQLite migration: flat fields → table columns.
- **2026-04-10:** Flat JSON files over SQLite for storage. One repo at a time, no cross-repo queries needed yet. SQLite earns its place when we need relationships or cross-repo querying.
- **2026-04-10:** SHA256 of the zip file bytes as the cache fingerprint. Computed after download, stored in `FRepoRecord`. Cache hit = `ZipSha256` field is populated. Deterministic — same zip always produces same SHA.
- **2026-04-10:** `FSaveDataPayload` stores `RepoOwner + RepoName` instead of duplicating raw GitHub JSON. Single source of truth is the `FRepoRecord` on disk.
- **2026-04-10:** Zip download via GitHub REST API (`/repos/{owner}/{repo}/zipball/HEAD`) instead of per-file GraphQL queries. Single HTTP request, all files available locally for any analysis.
- **2026-04-10:** Pipeline architecture for initialization rather than a monolithic function. Future steps (zip extraction, file analysis, terrain gen) plug in without changing existing code.
- **2026-04-10:** Zip stays on disk after metadata is saved. Future pipeline steps (extraction, file analysis, terrain gen) will post-process it. Deletion happens after all post-processing is complete (future milestone). Re-download only if record doesn't exist or is explicitly refreshed.

## Status

- [x] FRepoRecord
- [x] UGitHubDataCache migration (raw string → structured record)
- [ ] FSaveDataPayload migration (GitHubDataJson → RepoOwner/RepoName) — deferred, tracked in PLAN.md
- [x] UGameInitPipeline
- [x] UGameInitSubsystem
- [x] CheckCacheStep
- [x] DownloadZipStep
- [x] VerifyZipStep
- [x] SaveMetadataStep
- [x] UInitOverlayWidget (C++ base — Blueprint subclass required before end-to-end works)
- [x] Tests
- [ ] WBP_InitOverlay Blueprint (manual step)
- [ ] WBP_MainMenu Blueprint update (manual step — set InitOverlayClass)
