# Editor Tasks

Manual steps that must be completed in the Unreal Editor.

## Main Menu Setup

1. Create folder `Content/MainMenu/Blueprints/`
2. Create new level `Content/MainMenu/Lvl_MainMenu` — empty level, no sky, no lights (black background)
3. Create Widget Blueprint `Content/MainMenu/Blueprints/WBP_MainMenu` (parent class: `UMainMenuWidget`) — add buttons in the designer matching BindWidget names: `NewGameButton`, `ContinueButton`, `CombatButton`, `SideScrollingButton`, `PlatformingButton`, plus `MainMenuBox` and `VariantPickerBox` VerticalBoxes
4. Create Blueprint class `Content/MainMenu/Blueprints/BP_MainMenuGameMode` (parent class: `AMainMenuGameMode`)
   - In the Details panel, set **Main Menu Widget Class** to `WBP_MainMenu`
5. Open `Lvl_MainMenu`, go to World Settings, set **Game Mode Override** to `BP_MainMenuGameMode`
6. PIE to verify: black screen with "New Game" button, click → variant picker, pick one → loads variant level
