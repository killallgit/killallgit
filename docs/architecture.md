# KillAllGit — Architecture

Procedurally generated terrain driven by GitHub repository data. Built in Unreal Engine 5.7 C++.

## System Map

| System | Spec | Status | Description |
|--------|------|--------|-------------|
| Gameplay Variants | — | Demo/Debug only | UE scaffolding templates — not part of the shipping game |
| Main Menu & Save System | [main-menu-save-system](specs/main-menu-save-system.md) | Implemented | Main menu UI, variant picker, JSON-backed save system |
| GitHub Data Layer | [github-data-layer](specs/github-data-layer.md) | Implemented | Fetches, caches, and serves GitHub repo data |
| Game Levels | [game-levels](specs/game-levels.md) | In Progress | Playable level with Urban Nomad character and basic locomotion |
| Game Initialization | [game-initialization](specs/game-initialization.md) | In Progress | Repo zip download, SHA cache, init pipeline with progress overlay |
| Terrain Generation | — | Not started | Procedural terrain from repo data |

## Data Flow

```
GitHub GraphQL API → repo metadata → FRepoRecord
GitHub REST API   → zipball download → SHA256 + metadata → FRepoRecord
       ↓
 FRepoRecord (Saved/RepoData/{owner}_{repo}.json)
       ↓
 Game Initialization Pipeline (cache check → download → verify → save)
       ↓
 Terrain Generation (planned)
       ↓
 Game World
```

## Module Structure

Single module: `KillAllGit`. All C++ source lives under `Source/KillAllGit/`.

```
Source/KillAllGit/
  GameInit/                 ← Initialization pipeline, subsystem, steps
  GitHub/                   ← GitHub data layer, repo record
  GameLevels/               ← Game level character, controller
  SaveSystem/               ← Save data interface, providers, subsystem
  Tests/                    ← Automation tests (mirrors source structure)
  UI/                       ← Main menu, debug overlays, shared widgets
  Variant_Combat/           ← Combat gameplay variant (demo/debug only)
  Variant_SideScrolling/    ← Side-scrolling gameplay variant (demo/debug only)
  Variant_Platforming/      ← Platforming gameplay variant (demo/debug only)
```

> **Note:** `Variant_*` directories are UE scaffolding templates kept for debug/demo reference only. They are not part of the shipping game.
