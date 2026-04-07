# KillAllGit — Architecture

Procedurally generated terrain driven by GitHub repository data. Built in Unreal Engine 5.7 C++.

## System Map

| System | Spec | Status | Description |
|--------|------|--------|-------------|
| Gameplay Variants | — | Scaffolded | Combat, SideScrolling, Platforming templates (default UE scaffolding) |
| GitHub Data Layer | [github-data-layer](specs/github-data-layer.md) | Planned | Fetches, caches, and serves GitHub repo data |
| Terrain Generation | — | Not started | Procedural terrain from repo data |
| Main Menu | — | Not started | New Game / Continue flow, player data management |

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
  Tests/                    ← Automation tests (mirrors source structure)
  Variant_Combat/           ← Combat gameplay variant
  Variant_SideScrolling/    ← Side-scrolling gameplay variant
  Variant_Platforming/      ← Platforming gameplay variant
  UI/                       ← Shared UI (debug overlays, etc.)
```
