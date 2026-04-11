# PLAN — Game Initialization Pipeline (Milestone 1)

**Goal:** On new game, download the target repo's zip from GitHub, SHA256 it, cache the metadata, and print the size — all behind a progress overlay on the main menu.

**Spec:** `docs/specs/game-initialization.md`

---

## Phase 1: FRepoRecord + Cache/Subsystem Migration

Replace `FGitHubRepositoryData` with `FRepoRecord`. Evolve `UGitHubDataCache` from raw string I/O to structured record I/O. Update `UGitHubDataSubsystem` to use the new type. No parallel code paths — migrate in place.

### Create

- `Source/KillAllGit/GitHub/RepoRecord.h`
  - `FRepoRecord` USTRUCT — flat fields:
    - Identity: Owner, Name
    - API metadata: Description, Url, StargazerCount, ForkCount, CreatedAt, UpdatedAt, PrimaryLanguage, DefaultBranchName (same fields as current `FGitHubRepositoryData`)
    - Zip metadata: ZipSha256, ZipSizeBytes (`int64` — zips can exceed 2GB), DownloadedAt
  - `bool HasZipData() const` — returns `!ZipSha256.IsEmpty()`
  - `static FString MakeRecordId(Owner, Name)` — returns sanitized `owner_name`

### Delete

- `Source/KillAllGit/GitHub/GitHubTypes.h` — replaced entirely by `RepoRecord.h`

### Modify

- `Source/KillAllGit/GitHub/GitHubDataCache.h/.cpp`
  - Replace interface in place (not alongside):
    - `HasCache(RepoId)` → `HasRecord(Owner, Name)`
    - `ReadCache(RepoId) → FString` → `ReadRecord(Owner, Name) → FRepoRecord`
    - `WriteCache(RepoId, JsonString)` → `WriteRecord(const FRepoRecord&)`
  - `SetCacheDirectory` stays (tests need it)
  - Directory changes from `Saved/GitHubCache/` to `Saved/RepoData/`
  - Serialization: `FJsonObjectConverter::UStructToJsonObjectString` / `JsonObjectStringToUStruct` (struct ↔ JSON for free, no manual field serialization)
  - `SanitizeKey` reused internally, takes `Owner + "/" + Name` as before

- `Source/KillAllGit/GitHub/GitHubDataSubsystem.h/.cpp`
  - `FOnRepositoryDataReceived` delegate type: `FGitHubRepositoryData` → `FRepoRecord`
  - **Two JSON formats — do not conflate:**
    - `ParseResponse(JsonString)` keeps doing manual GraphQL-envelope unwrapping (`data.repository` → flat fields). It populates an `FRepoRecord` (Owner left empty — caller sets it). This handles **raw GitHub API response JSON**.
    - `ReadRecord`/`WriteRecord` on the cache use `FJsonObjectConverter` for **flat struct JSON**. These are different formats — `ParseResponse` never touches `FJsonObjectConverter`.
  - `RequestRepositoryData(Owner, Name, bForceRefresh, OnComplete)`:
    - Cache hit: `ReadRecord(Owner, Name)` → `FRepoRecord` directly (no re-parsing — it's already a struct)
    - Cache miss: fetch → `ParseResponse` → set `Record.Owner = Owner; Record.Name = Name` → `WriteRecord` → delegate
  - `LogRepositoryData` param type → `FRepoRecord`
  - Add facade methods for init pipeline (cache stays private):
    - `FRepoRecord GetRepoRecord(Owner, Name)` — reads from cache, returns empty record if not found
    - `void UpdateRepoRecord(const FRepoRecord&)` — read-modify-write: reads existing record, merges non-empty fields from input, writes back. **Note:** safe because all callers run on game thread and re-entrancy is guarded at the UI level.
    - `bool HasRepoZipData(Owner, Name)` — reads record, returns `HasZipData()`

- `Source/KillAllGit/KillAllGit.Build.cs`
  - Add `"KillAllGit/GameInit"` and `"KillAllGit/Tests/GameInit"` to include paths

- `Source/KillAllGit/GitHub/GitHubAPIClient.h/.cpp`
  - Extract `ResolveAuthToken()` → **free function** in a new header `Source/KillAllGit/GitHub/GitHubAuth.h` (e.g. `namespace GitHubAuth { FString ResolveToken(); }`). It has no instance state — reads env var + file. Keeps `UGitHubAPIClient` clean and avoids coupling the init subsystem to the API client class.
  - `UGitHubAPIClient::FetchRepository` calls `GitHubAuth::ResolveToken()` internally (no behavior change).

- `Source/KillAllGit/UI/MainMenuWidget.cpp`
  - Update include: `GitHubTypes.h` → `RepoRecord.h`
  - No logic changes — `OnRefreshClicked` passes an empty delegate, doesn't consume the type

### Test

- `Source/KillAllGit/Tests/GitHub/GitHubDataCacheTest.cpp`
  - Rewrite for new interface: write `FRepoRecord` → read back → verify all fields round-trip
  - `HasRecord` returns false when no file, true after write
  - Test with `SetCacheDirectory` for isolation

- `Source/KillAllGit/Tests/GitHub/GitHubDataSubsystemTest.cpp`
  - Same test logic, `FGitHubRepositoryData` → `FRepoRecord` in assertions
  - Add assertion for Owner/Name being empty (ParseResponse doesn't set them)

### Commit checkpoint: `task check` must pass

---

## Phase 2: Init Pipeline

Generic sequential async step runner. No GitHub-specific code — just runs named steps that report progress.

### Create

- `Source/KillAllGit/GameInit/GameInitPipeline.h/.cpp`
  - `UObject` subclass — created via `NewObject<UGameInitPipeline>(OwningSubsystem)` so GC tracks lifetime through the subsystem outer chain
  - `FInitStepEntry` — struct: `FString Label`, `TFunction<void(TFunction<void(float)> ReportProgress, TFunction<void()> OnStepDone)> Execute`
  - `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInitProgress, const FString&, StepLabel, float, Fraction)`
  - `DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInitComplete)`
  - `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInitFailed, const FString&, Reason)`
  - `void AddStep(FString Label, ExecuteFunc)` — enqueues a step
  - `void Run()` — starts executing steps sequentially. If no steps enqueued, fires OnComplete immediately.
  - `void Complete()` — short-circuit: skip remaining steps, fire OnComplete (used by cache hit)
  - `void Fail(FString Reason)` — abort pipeline, fire OnInitFailed (used on download error, bad token, etc.). Sets `bAborted` flag so in-flight step completions are ignored.
  - `void Cancel()` — abort without firing any delegate (used on teardown/navigation away). Sets `bAborted`.
  - `bool IsRunning() const` — true between `Run()` and terminal state
  - `FOnInitProgress OnProgress` — broadcast each time a step reports
  - `FOnInitComplete OnComplete` — broadcast when all steps finish
  - `FOnInitFailed OnFailed` — broadcast on abort
  - Internal: step index, `bAborted` flag, advances to next step when current calls OnStepDone (no-op if `bAborted`)

### Test

- `Source/KillAllGit/Tests/GameInit/GameInitPipelineTest.cpp`
  - Test: empty pipeline fires OnComplete immediately on Run()
  - Test: single step runs, reports progress, fires OnComplete
  - Test: three steps run in order (verify via appending to a TArray<FString>)
  - Test: progress delegate fires with correct label and fraction
  - Test: `Complete()` skips remaining steps, fires OnComplete
  - Test: `Fail(Reason)` aborts pipeline, fires OnFailed with reason string
  - Test: `Cancel()` aborts pipeline, fires no delegates
  - Test: step completing after `Fail()` was called is a no-op (no crash, no advance)

### Commit checkpoint: `task check` must pass

---

## Phase 3: Init Steps

The four concrete steps for this milestone. Each is a **static function** on `UGameInitSubsystem` (testable in isolation), wired into the pipeline by `BeginInit`. Steps read/write `CurrentRecord` on the subsystem — no lambda-captured locals.

**Thread safety invariant:** All step callbacks (HTTP completion, progress) execute on the game thread. `CurrentRecord` mutation is safe without locks because of this. Do not use `AsyncTask`/`Async()` in steps without revisiting this.

### Auth token sharing

`GitHubAuth::ResolveToken()` (extracted in Phase 1) — free function, no class dependency. The zip download step calls it directly.

### Step 1 — CheckCache

- Calls `UGitHubDataSubsystem::HasRepoZipData(Owner, Name)`
- If true → `Pipeline->Complete()` (short-circuits remaining steps)
- If false → report 100% and advance

### Step 2 — DownloadZip

- HTTP GET to `https://api.github.com/repos/{Owner}/{Name}/zipball/HEAD`
- Auth via `GitHubAuth::ResolveToken()`
- **Lambda safety:** capture `TWeakObjectPtr<UGameInitSubsystem>` (not raw `this`). Check validity via `WeakThis.Pin()` before accessing `CurrentRecord` or `Pipeline` in the HTTP callback. Pattern from UE 5.7 docs:
  ```
  [WeakSelf = MakeWeakObjectPtr(this)](...)
  {
      if (TStrongObjectPtr<UGameInitSubsystem> Self = WeakSelf.Pin())
      {
          // safe to access Self->CurrentRecord, Self->Pipeline
      }
  }
  ```
- Store the `TSharedPtr<IHttpRequest>` on the subsystem so `Cancel()` can call `Request->CancelRequest()`
- Bind `OnRequestProgress` → report `BytesReceived / ContentLength` as fraction
- Save response body to `Saved/RepoData/{owner}_{name}.zip`
- Error handling:
  - No auth token → `Pipeline->Fail("No GitHub token found")`
  - HTTP error (404, 403, network) → `Pipeline->Fail(reason)`
  - Success → advance to next step

### Step 3 — VerifyZip

- Read the zip file bytes from disk
- Compute SHA256 hash (use UE5 crypto — determine exact API during implementation)
- Set `CurrentRecord.ZipSha256` and `CurrentRecord.ZipSizeBytes` (`int64`) on the subsystem
- Report 100% → advance

### Step 4 — SaveMetadata

- Set `CurrentRecord.DownloadedAt` (FDateTime::UtcNow ISO 8601)
- Write via `UGitHubDataSubsystem::UpdateRepoRecord(CurrentRecord)`:
  - Reads existing record (preserves any API metadata from prior Refresh)
  - Merges zip fields from CurrentRecord
  - Writes the merged record back
- Zip file stays at `Saved/RepoData/{owner}_{name}.zip` — future pipeline steps will post-process it
- `UE_LOG(LogKillAllGit, Display, TEXT("Zip size: %lld bytes"), CurrentRecord.ZipSizeBytes)`
- Report 100% → advance → pipeline fires OnComplete

### Step implementation pattern

Steps are static functions on `UGameInitSubsystem`, called from lambdas in `BeginInit`:

```
static void ExecuteCheckCache(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline, ...);
static void ExecuteDownloadZip(UGameInitSubsystem* Self, UGameInitPipeline* Pipeline, ...);
// etc.
```

`BeginInit` wires them — lambdas capture `TWeakObjectPtr<UGameInitSubsystem>` and pin before calling the static function:
```
Pipeline->AddStep("Checking cache...", [WeakSelf = MakeWeakObjectPtr(this), Pipeline](...) {
    if (auto Self = WeakSelf.Pin()) { ExecuteCheckCache(Self.Get(), Pipeline, ...); }
});
```

### Create

- `Source/KillAllGit/GameInit/GameInitSubsystem.h/.cpp`
  - `UGameInstanceSubsystem`
  - `Initialize()` calls `Collection.InitializeDependency<UGitHubDataSubsystem>()` — ensures GitHub subsystem is ready
  - `void BeginInit(Owner, Name, OnComplete delegate)` — builds the step list, creates pipeline via `NewObject<UGameInitPipeline>(this)`, runs it
  - `void CancelInit()` — cancels in-flight pipeline and HTTP request. Called from `Deinitialize()` override and from UI on navigation away.
  - Owns the `UGameInitPipeline` via `UPROPERTY() TObjectPtr<UGameInitPipeline>`
  - Stores active HTTP request as `TSharedPtr<IHttpRequest>` for cancellation
  - `FRepoRecord CurrentRecord` member — shared state that steps read/write. Steps capture weak `this`, not loose locals.
  - Exposes `OnProgress` / `OnComplete` / `OnFailed` for UI binding
  - Accesses repo records through `UGitHubDataSubsystem` facade methods (GetRepoRecord, UpdateRepoRecord, HasRepoZipData) — never touches the cache directly
  - On `BeginInit`: if a pipeline already exists and `IsRunning()`, cancel it first before creating a new one

### Test

- `Source/KillAllGit/Tests/GameInit/GameInitStepsTest.cpp`
  - Test: CheckCache with no record → does not call Complete()
  - Test: CheckCache with existing record (HasZipData=true) → calls Complete()
  - Test: VerifyZip produces consistent SHA256 for known bytes
  - Test: SaveMetadata writes record, zip file persists, record preserves existing API metadata
  - Test: DownloadZip with missing token → calls Fail()

### Commit checkpoint: `task check` must pass

---

## ⛔ STOP — Blueprint Required: Init Overlay Widget

**Phase 4 creates the C++ base class. Phase 5 depends on a Blueprint subclass that must be created manually in the Unreal Editor before Phase 5 can work.**

After Phase 4 compiles, pause and create the Blueprint before starting Phase 5.

---

## Phase 4: Init Overlay Widget (C++ only)

Progress bar overlay on the main menu. The C++ class is `UCLASS(abstract)` — a Blueprint subclass provides the visual layout.

### Create

- `Source/KillAllGit/UI/InitOverlayWidget.h/.cpp`
  - `UCLASS(abstract)` — Blueprint subclass required for layout (matches `UMenuButton`, `AMainMenuGameMode` pattern)
  - `UUserWidget` subclass
  - `BindWidget` members:
    - `UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> ProgressBar`
    - `UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_StepLabel`
  - `void BindToPipeline(UGameInitPipeline* Pipeline)` — subscribes to OnProgress / OnComplete / OnFailed
  - OnProgress → set bar percent (`ProgressBar->SetPercent(Fraction)`), set label text
  - OnComplete → `RemoveFromParent()` (self-removes)
  - OnFailed → `RemoveFromParent()` (error logged via UE_LOG, no error display in overlay for M1)

### No tests for UI — verified visually

### Commit checkpoint: `task compile` must pass (no automation test for widget)

---

## ⛔ STOP — Manual Blueprint Steps Required

**Both blueprints below must be created in the Unreal Editor before Phase 5 can run. Phase 5 will crash or silently fail without them.**

### Blueprint 1: WBP_InitOverlay

Create the overlay widget Blueprint that the C++ class drives.

1. **Content Browser** → right-click → **User Interface → Widget Blueprint**
2. In the dialog, select **InitOverlayWidget** (the C++ class from Phase 4) as the **Parent Class**
   - If it doesn't appear, close and reopen the editor after compiling Phase 4
3. Name it **`WBP_InitOverlay`**, save in **`Content/UI/`**
4. Open the Widget Blueprint designer and build this layout:

```
[Canvas Panel]                          ← root (auto-created)
  └─ [Border]                           ← full-screen background
       Name: anything
       Anchors: stretch-to-fill (top-left 0,0 → bottom-right 1,1)
       Offsets: all 0
       Brush Color: (0, 0, 0, 0.6)     ← semi-transparent black
       └─ [Vertical Box]               ← centered content
            Anchors: center (0.5, 0.5)
            Alignment: (0.5, 0.5)       ← pivot at center
            Is Variable: false
            Size: Auto
            ├─ [Progress Bar]
            │    Name: ProgressBar      ← MUST match BindWidget name exactly
            │    Size X: 400
            │    Percent: 0.0
            │    Fill Color: green or accent color of your choice
            └─ [Text Block]
                 Name: Txt_StepLabel    ← MUST match BindWidget name exactly
                 Text: "Initializing..."
                 Justification: Center
                 Font Size: 14
                 Color: white
                 Padding Top: 8
```

5. **Compile** and **Save** the widget Blueprint

**Verification:** Open the Blueprint and confirm no yellow "BindWidget" warnings in the compiler results. If you see warnings, the widget names don't match the C++ `BindWidget` declarations.

### Blueprint 2: Update WBP_MainMenu

Expose the overlay class reference so C++ can spawn it.

1. Open **`Content/UI/WBP_MainMenu`** (the existing main menu widget Blueprint)
2. In the **Details** panel (with the root selected, or via Class Defaults):
   - Find the property **`Init Overlay Class`** (added by Phase 5 C++ changes — compile first)
   - Set it to **`WBP_InitOverlay`** (the Blueprint from step above)
3. **Compile** and **Save**

**If `Init Overlay Class` doesn't appear:** Verify Phase 5 C++ compiled successfully. The property is `UPROPERTY(EditAnywhere)` on `UMainMenuWidget`. Close and reopen the Blueprint editor after recompiling.

### After both blueprints are done, continue to Phase 5 integration testing.

---

## Phase 5: Integration

Wire the init pipeline into the main menu flow.

### Modify

- `Source/KillAllGit/UI/MainMenuWidget.h/.cpp`
  - Add `UPROPERTY(EditAnywhere, Category = "Init") TSubclassOf<UInitOverlayWidget> InitOverlayClass` — set in Blueprint (see manual step above)
  - Add `TObjectPtr<UInitOverlayWidget> InitOverlay` — created fresh each init (destroyed on level transition via RemoveFromParent)
  - Add `EGameVariant PendingVariant` member
  - Add `bool bInitInProgress = false` — re-entrancy guard
  - Extract `bool ParseRepoInput(FString& OutOwner, FString& OutName)` — parses `Input_Repo` text, validates `owner/name` format. Used by both `OnRefreshClicked` and `StartGame`.
  - Refactor `OnRefreshClicked` to use `ParseRepoInput`
  - Change `StartGame(Variant, bIsNewGame)`:
    - If `bInitInProgress` → return (ignore double-clicks while pipeline runs)
    - If `bIsNewGame`:
      - `ParseRepoInput` → if invalid, log warning and return
      - **Guard:** if `InitOverlayClass` is null, `UE_LOG(Error)` and return — don't crash on missing Blueprint config
      - Set `PendingVariant = Variant`, `bInitInProgress = true`
      - Disable variant buttons + Refresh button
      - Get `UGameInitSubsystem`, call `BeginInit(Owner, Name, ...)`
      - Create overlay: `InitOverlay = CreateWidget<UInitOverlayWidget>(GetOwningPlayer(), InitOverlayClass)`
      - Add to viewport: `InitOverlay->AddToViewport(1)` — Z-order 1 (menu is at Z-order 0), true overlay
      - `InitOverlay->BindToPipeline(Pipeline)`
    - If not new game (Continue): existing flow unchanged — `OpenLevel` directly
  - Add `void OnInitComplete()`:
    - `bInitInProgress = false`
    - Null-check and remove overlay: `if (InitOverlay) { InitOverlay->RemoveFromParent(); InitOverlay = nullptr; }`
    - `SaveSubsystem->CreateSaveData(PendingVariant, FMockGitHubData::GetMockJson())` — mock data kept until FSaveDataPayload migration (tracked in spec, not in this milestone)
    - `OpenLevel(PendingVariant map path)`
  - Add `void OnInitFailed()`:
    - `bInitInProgress = false`
    - Null-check and remove overlay: `if (InitOverlay) { InitOverlay->RemoveFromParent(); InitOverlay = nullptr; }`
    - Re-enable variant buttons + Refresh button

### Flow after integration

```
Variant button → OnVariantSelected(Variant)
  → bInitInProgress? → yes: ignore
  → ParseRepoInput → invalid: log warning, return
  → InitOverlayClass null? → log error, return
  → PendingVariant = Variant
  → bInitInProgress = true, disable buttons
  → UGameInitSubsystem::BeginInit(Owner, Name)
  → Create + show InitOverlay (Z-order 1)
  → Pipeline runs steps (overlay shows progress)
  → OnComplete:
      → bInitInProgress = false
      → Remove + null overlay
      → Print zip size to console
      → SaveSubsystem->CreateSaveData(PendingVariant, MockJson)
      → OpenLevel(PendingVariant map path)
  → OnFailed:
      → bInitInProgress = false
      → Remove + null overlay
      → Re-enable buttons
      → (error already logged via UE_LOG)
```

### Manual verification

- New game with fresh repo → overlay shows, download happens, size prints, level loads
- New game with same repo → overlay shows briefly (cache hit), level loads
- Invalid repo format → warning logged, stays on menu
- Valid repo but download fails (bad token, 404) → overlay hides, buttons re-enabled, error logged
- Double-click variant button → second click ignored while pipeline runs
- Navigate away mid-download → pipeline cancelled, no crash

### Commit checkpoint: `task check` must pass

---

## Deferred (not in this milestone)

These items are tracked in the spec but intentionally excluded from this plan:

- **FSaveDataPayload migration** — replace `GitHubDataJson` with `RepoOwner + RepoName` reference. Blocked on: deciding when mock data is replaced with real data.
- **User-visible error feedback** — overlay currently just disappears on failure. Future: show error text briefly before hiding.
