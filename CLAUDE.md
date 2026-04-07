# KillAllGit

Unreal Engine 5.7 C++ project. Procedurally generates terrain from GitHub repository data.

## Build

```bash
# Build from command line (Mac)
# Open in Unreal Editor for most workflows
```

## Project Structure

```
Source/KillAllGit/          C++ source (single module)
Content/                    Unreal assets, blueprints, maps
Config/                     Engine and project configuration
docs/                       Architecture and design specs
  architecture.md           System map — the entry point
  specs/                    One spec per system
```

## Rules

### Design Docs

- **`docs/architecture.md` is the entry point.** Read it first to orient. If a system isn't linked from architecture.md, it doesn't exist from a planning perspective.
- **One spec per system, not per task.** When adding features to an existing system, update its spec — don't create a new file.
- **Specs describe WHAT and WHY, not HOW.** No function signatures or line-by-line logic. Implementation detail lives in code.
- **Every spec has a file map.** Lists which source files implement the spec. If a class exists in code but isn't in any spec's file map, it's untracked.
- **Every spec has a decisions log.** Dated entries: "we chose X over Y because Z." Prevents re-litigating old decisions.
- **Update specs before marking work done.** When you finish implementing something, update the spec's status and file map. This is a checklist item, same as running tests.

### Code Patterns

- All gameplay classes use `UCLASS(abstract)` — Blueprint subclasses are the delivery mechanism.
- Subsystems (`UGameInstanceSubsystem`) for game-wide services. No singletons, no global state.
- Interfaces for cross-system communication. No direct casts in hot paths.
- `DoX()` virtual BlueprintCallable wrappers for input actions (supports hardware + touch).
- Each variant (Combat, SideScrolling, Platforming) is self-contained in its own subfolder.

### Testing

- Use Unreal Automation Framework (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`).
- Test files live alongside implementation: `MyClass.test.cpp` next to `MyClass.cpp`.

### Naming

- Prefix all GitHub data layer classes with their domain: `UGitHubAPIClient`, `UGitHubDataCache`, etc.
- UStructs for data transfer: `FGitHubRepositoryData`, etc.
- GraphQL query files: `Content/Queries/<QueryName>.graphql`
