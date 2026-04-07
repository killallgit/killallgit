# KillAllGit — Architecture

Procedurally generated terrain driven by GitHub repository data. Built in Unreal Engine 5.7 C++.

## System Map

| System | Spec | Status | Description |
|--------|------|--------|-------------|
| Gameplay Variants | — | Scaffolded | Combat, SideScrolling, Platforming templates (default UE scaffolding) |
| Main Menu & Save System | [main-menu-save-system](specs/main-menu-save-system.md) | Implemented | Main menu UI, variant picker, JSON-backed save system |
| GitHub Data Layer | [github-data-layer](specs/github-data-layer.md) | Planned | Fetches, caches, and serves GitHub repo data |
| Terrain Generation | — | Not started | Procedural terrain from repo data |

## Data Flow

```
GitHub GraphQL API
       ↓
 GitHub Data Layer (fetch + cache)
       ↓
 Terrain Generation (planned)
       ↓
 Game World
```

## Module Structure

Single module: `KillAllGit`. All C++ source lives under `Source/KillAllGit/`.

```
Source/KillAllGit/
  GitHub/                   ← GitHub data layer
  SaveSystem/               ← Save data interface, providers, subsystem
  Tests/                    ← Automation tests (mirrors source structure)
  UI/                       ← Main menu, debug overlays, shared widgets
  Variant_Combat/           ← Combat gameplay variant
  Variant_SideScrolling/    ← Side-scrolling gameplay variant
  Variant_Platforming/      ← Platforming gameplay variant
```
