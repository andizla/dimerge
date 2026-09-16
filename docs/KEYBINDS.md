# SnowRunner keybinds on the merged wheel

Generated 2026-09-16 by tools/keybinds/keybinds.py from C:/Program Files (x86)/Steam/steamapps/common/SnowRunner5\preload\paks\client\initial.pak, C:/Program Files (x86)/Steam/steamapps/common/SnowRunner5/Sources/Bin\dimerge.log and c:/program files (x86)/steam\userdata\72265044\1465360\remote\user_settings.cfg. Run it again after changing bindings or the ini.

## Merged key numbers

The game stores a wheel binding as a key number: buttons 0 to 127, axes 128 to 135 (X, Y, Z, Rx, Ry, Rz, Slider0, Slider1), hat directions 136 to 139 (up, right, down, left). What the proxy put on each number in its last run:

| Number | Physical control |
|---|---|
| 8 to 21 | GX100 buttons 0 to 13 |
| 23 | GX100 button 15 |
| 24 to 37 | GX100 buttons 0 to 13 with its button 14 held |
| 39 | GX100 button 15 with its button 14 held |
| 40 to 45 | PXNCB1 buttons 0 to 5 |
| 48 to 65 | PXNCB1 buttons 8 to 25 |
| 66 | PXNCB1 hat 0 up |
| 67 | PXNCB1 hat 0 right |
| 68 | PXNCB1 hat 0 down |
| 69 | PXNCB1 hat 0 left |
| 70 to 75 | PXNCB1 buttons 0 to 5 with its button 7 held |
| 78 to 95 | PXNCB1 buttons 8 to 25 with its button 7 held |
| 96 to 101 | PXNCB1 buttons 0 to 5 with its button 6 held |
| 104 to 121 | PXNCB1 buttons 8 to 25 with its button 6 held |
| 128 | wheelbase 0483:0522 axis X |
| 129 | SIMSONNPlusX axis X (Y) |
| 130 | SIMSONNPlusX axis Y (Z) |
| 133 | SIMSONNPlusX axis Z (Rz) |
| 136 | wheelbase 0483:0522 hat up |
| 137 | wheelbase 0483:0522 hat right |
| 138 | wheelbase 0483:0522 hat down |
| 139 | wheelbase 0483:0522 hat left |
| other 0 to 127 | wheelbase 0483:0522 buttons, native |

## Binding slots of a custom wheel

Every slot the settings menu offers for a wheel the game does not know by vendor id. Slots marked added come from the pak patch (`dimerge-setup pak` or tools/pakpatch/wheel_slots.py): the stock game has no wheel slot for moving the crane, entering crane mode, attaching cargo, the anchor or the HUD toggle, and its engine slot is empty until the patch completes it.

| Slot | Menu name | Game input | Context | Bound now |
|---|---|---|---|---|
| SteeringWheel | Steering Wheel Axis | Truck.SteeringWheel | GAME | 128 = wheelbase 0483:0522 axis X |
| Throttle | Throttle Pedal | Truck.WheelThrottle | GAME | 129 = SIMSONNPlusX axis X (Y) |
| Brake | Brake | Truck.WheelBrake | GAME | 130 = SIMSONNPlusX axis Y (Z) |
| Clutch | Clutch Pedal | Truck.ClutchPedal | GAME | 133 = SIMSONNPlusX axis Z (Rz) |
| Headlights | Headlight | Truck.HeadlightSolo | GAME | 44 = PXNCB1 button 4 |
| Honk | Honk | Truck.Honk | GAME |  |
| AWD | AWD | Truck.AwdSolo | GAME | 52 = PXNCB1 button 12 |
| DiffLock | Diff. Lock | Truck.DiffLockSolo | GAME | 53 = PXNCB1 button 13 |
| Handbrake | Handbrake | Truck.Handbrake | GAME | 51 = PXNCB1 button 11 |
| QuickWinch | Quick Winch | Truck.QuickWinch, Truck.PullWinch | GAME | 43 = PXNCB1 button 3 |
| StartEngine (added) | Engine | Truck.EngineSolo | GAME | 50 = PXNCB1 button 10 |
| Minimap | Navigation Map | Exploration.ShowMinimap | GAME | 65 = PXNCB1 button 25 |
| FunctionMenu | Functions | Truck.Functions | GAME | 64 = PXNCB1 button 24 |
| IngameMenu | Pause Menu | UI.ToggleMenu, Exploration.Menu, UI.ShowMenu | UI |  |
| OpenPlayerProfile | Player's Profile | Exploration.PlayerProfile | GAME |  |
| DpadUp | Up | UI.GamepadDPadUp, Gamepad.GamepadDPadUp, Exploration.TransmissionUp, Truck.SelectTopWinchPoint, Exploration.TransmissionInput | FUNCTION_MENU | 66 = PXNCB1 hat 0 up |
| DpadDown | Down | UI.GamepadDPadDown, Gamepad.GamepadDPadDown, Exploration.TransmissionDown, Truck.SelectBottomWinchPoint, Exploration.TransmissionInput backward | FUNCTION_MENU | 68 = PXNCB1 hat 0 down |
| DpadLeft | Left | UI.GamepadDPadLeft, Gamepad.GamepadDPadLeft, Exploration.TransmissionLeft, Truck.SelectLeftWinchPoint, Exploration.TransmissionInput left | TUTORIAL | 139 = wheelbase 0483:0522 hat left |
| DpadRight | Right | UI.GamepadDPadRight, Gamepad.GamepadDPadRight, Exploration.TransmissionRight, Truck.SelectRightWinchPoint, Exploration.TransmissionInput right | TUTORIAL | 137 = wheelbase 0483:0522 hat right |
| UIAccept | Accept | UI.Accept, Gamepad.GamepadA, Popups.Accept, Popups.Load, Popups.RefuelRepair, Popups.FillWaterStation, Exploration.SkipCinematic, UI.Enter, UI.Accept, UI.AcceptTap, UI.TruckDeploy, UI.FocusSubstage, UI.InviteToParty, UI.ShowPlayerProfile, Gamepad.Enter, Gamepad.Accept, Popups.AcceptTap, Popups.AcceptBuySellTrailer, Truck.RemoveCargo | FUNCTION_MENU | 49 = PXNCB1 button 9 |
| UIDecline | Decline | UI.Back, Gamepad.Back, Popups.Back, Truck.ReleaseWinch, Popups.Close, Truck.CloseFunctions, Truck.StopRemoveCargo | CUSTOM_ADDON_ACTION | 48 = PXNCB1 button 8 |
| UITabLeft | UI Left | UI.TabBarPrevTab, UI.MinimapPrevTab, UI.TabBtnL, UI.OpenGuides, UI.PrevPage, Popups.tabLeft, Popups.SwitchCargoLeft, UI.SelectSettingLeft | TUTORIAL | 69 = PXNCB1 hat 0 left |
| UITabRight | UI Right | UI.TabBarNextTab, UI.MinimapNextTab, UI.TabBtnR, UI.NextPage, Popups.tabRight, Popups.SwitchCargoRight, UI.SelectSettingRight | TUTORIAL | 67 = PXNCB1 hat 0 right |
| TruckCameraRotationX | Camera Rotation X | DriveCamera.WheelCameraRotationX, UI.WheelCameraRotationX | GAME |  |
| TruckCameraRotationY | Camera Rotation Y | DriveCamera.WheelCameraRotationY, UI.WheelCameraRotationY | GAME |  |
| TruckCameraZoom | Camera Zoom | DriveCamera.WheelCameraZoom | GAME |  |
| MinimapCameraMovementX | Minimap Camera Movement Axis X | UI.WheelMinimapMovementX | UI |  |
| MinimapCameraMovementY | Minimap Camera Movement Axis Y | UI.WheelMinimapMovementY | UI |  |
| GearHigh | High Gear | Exploration.SetGearHigh | GAME |  |
| GearAuto | Auto | Exploration.SetGearAuto | GAME |  |
| GearNeutral | Neutral | Exploration.SetGearNeutral | GAME |  |
| GearLow1 | Low - | Exploration.SetGearLow1 | GAME |  |
| GearLow2 | Low | Exploration.SetGearLow2 | GAME |  |
| GearLow3 | Low + | Exploration.SetGearLow3 | GAME |  |
| GearReverse | Reverse | Exploration.SetGearReverse | GAME |  |
| CraneWinchLift | Lift Crane Winch | Crane.WinchActions | CRANE | 54 = PXNCB1 button 14 |
| CraneWinchLower | Lower Crane Winch | Crane.WinchActions backward | CRANE | 55 = PXNCB1 button 15 |
| CraneWinchRotateLeft | Pull Crane Winch to the left | Crane.WinchActions left | CRANE | 56 = PXNCB1 button 16 |
| CraneWinchRotateRight | Pull Crane Winch to the right | Crane.WinchActions right | CRANE | 57 = PXNCB1 button 17 |
| CraneArrowLift | Lift Crane Arrow | Crane.moveYPlane | CRANE | 58 = PXNCB1 button 18 |
| CraneArrowLower | Lower Crane Arrow | Crane.moveYPlane backward | CRANE | 59 = PXNCB1 button 19 |
| CraneMoveForward (added) | Move Crane Forward | Crane.moveXZplane | CRANE | 66 = PXNCB1 hat 0 up |
| CraneMoveBackward (added) | Move Crane Backward | Crane.moveXZplane backward | CRANE | 68 = PXNCB1 hat 0 down |
| CraneMoveLeft (added) | Move Crane Left | Crane.moveXZplane left | CRANE | 69 = PXNCB1 hat 0 left |
| CraneMoveRight (added) | Move Crane Right | Crane.moveXZplane right | CRANE | 67 = PXNCB1 hat 0 right |
| CraneTurnOn (added) | Enter Crane Mode | Crane.TurnOn | GAME | 58 = PXNCB1 button 18 |
| CraneAttachCargo (added) | Attach or Detach Cargo | Crane.AttachCargo | CRANE | 52 = PXNCB1 button 12 |
| CraneAnchor (added) | Crane Anchor | Crane.EnableAnchor | CRANE | 53 = PXNCB1 button 13 |
| GarageGlobalMap | Move to Global Map from Garage | UI.GoToGlobalMap | GARAGE_SLOT_MENU | 121 = PXNCB1 button 25 with its button 6 held |
| GaragePlayerProfile | Player's Profile | UI.PlayerProfile | GARAGE_SLOT_MENU |  |
| GarageLocalMap | Navigation Map | UI.LocalMap | GARAGE_SLOT_MENU |  |
| GarageRetain | Retain | UI.TruckRetain | GARAGE_SLOT_MENU |  |
| GarageWarehouseSellTruck | Sell Truck | UI.TruckSell, UI.AddonInstall | GARAGE_WAREHOUSE |  |
| GarageWarehouseDeployTruck | Deploy | UI.TruckDeploy | GARAGE_WAREHOUSE |  |
| GarageShopSwapDescAndStats | Show description/characteristics | UI.SwapDescAndStatsVisibility | GARAGE_SHOP |  |
| GarageShopPreviewTruck | Preview | UI.TruckPreview, UI.ExtraTruckPurchase | GARAGE_SHOP |  |
| GarageGlobalMapGoToGarage | Move to Garage from Global Map | UI.GlobalMapGoToGarage | GARAGE_GLOBAL_MAP |  |
| MinimapSkipTime | Skip Time | UI.SkipTime | MINIMAP |  |
| MinimapAcceptContract | Accept Contract | UI.AcceptContract | MINIMAP |  |
| MinimapRestartContract | Restart Contract | UI.RestartContract | MINIMAP |  |
| MinimapGlobalMap | Move to Global Map from Minimap | UI.GoToRegionalMap | MINIMAP |  |
| MinimapGoToPlayer | Go to Player | UI.MoveToPlayer | MINIMAP |  |
| MinimapAddPathPoint | Add Destination Point | UI.PlacePathMarker | MINIMAP |  |
| MinimapClearPathPoint | Clear Last Point | UI.ClearPath, UI.ClearLastPathPoint | MINIMAP |  |
| TutorialAccept | Close tutorial hints | UI.AcceptTutorial | TUTORIAL |  |
| PopupOpen | Close/open UI zones | Exploration.StartCargoManagement | UI |  |
| UILeft2 | UI Left (additional) | Popups.triggerLeft, Popups.SwitchPlaceLeft | UI |  |
| UIRight2 | UI Right  (additional) | Popups.triggerRight, Popups.SwitchPlaceRight | UI |  |
| ToggleGameCamera | Toggle Camera / [HOLD] Show HUD in Immersive Mode | Truck.ToggleCamera | GAME |  |
| ShowDamage | Show damage zones | Truck.ShowDamageMarkers | FUNCTION_MENU |  |
| PopupAdditionalButton | Additional functions in pop-ups | Popups.Details |  |  |
| CameraRotateStepLeft | Turn camera left | DriveCamera.RotateStepLeft | GAME |  |
| CameraRotateStepRight | Turn camera right | DriveCamera.RotateStepRight | GAME |  |
| CustomAddonAction1 | Custom addon action 1 | CustomAddons.action_1 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction2 | Custom addon action 2 | CustomAddons.action_2 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction3 | Custom addon action 3 | CustomAddons.action_3 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction4 | Custom addon action 4 | CustomAddons.action_4 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction5 | Custom addon action 5 | CustomAddons.action_5 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction6 | Custom addon action 6 | CustomAddons.action_6 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction7 | Custom addon action 7 | CustomAddons.action_7 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction8 | Custom addon action 8 | CustomAddons.action_8 | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction9Forward | Custom vector addon action 9 (forward) | CustomAddons.action_9_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction9Backward | Custom vector addon action 9 (backward) | CustomAddons.action_9_vector backward | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction10Forward | Custom vector addon action 10 (forward) | CustomAddons.action_10_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction10Backward | Custom vector addon action 10 (backward) | CustomAddons.action_10_vector backward | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction11Forward | Custom vector addon action 11 (forward) | CustomAddons.action_11_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction11Backward | Custom vector addon action 11 (backward) | CustomAddons.action_11_vector backward | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction12Forward | Custom vector addon action 12 (forward) | CustomAddons.action_12_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction12Backward | Custom vector addon action 12 (backward) | CustomAddons.action_12_vector backward | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction13Forward | Custom vector addon action 13 (forward) | CustomAddons.action_13_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction13Backward | Custom vector addon action 13 (backward) | CustomAddons.action_13_vector backward | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction14Forward | Custom vector addon action 14 (forward) | CustomAddons.action_14_vector | CUSTOM_ADDON_ACTION |  |
| CustomAddonAction14Backward | Custom vector addon action 14 (backward) | CustomAddons.action_14_vector backward | CUSTOM_ADDON_ACTION |  |

## Every input link in the game data

All Context.Action names the game data mentions (HUD hints, presets, menus). A wheel slot targets one of these; anything without a slot above is keyboard or gamepad only for a custom wheel unless a slot is added the way the pak patch does it.

- **Crane**: AttachCargo, CargoRotationAxis, EnableAnchor, ExitAdvance, FakeMoveD, FakeMoveF, FakeMoveL, FakeMoveR, HideNavLegend, MoveDown, MoveUp, MoveXAxis, MoveYAxis, MoveZAxis, TurnClockwise, TurnCounterClockwise, TurnOn, WinchActions, WinchPull, WinchPush, moveXZplane, moveYPlane

- **CustomAddons**: action_1, action_10_vector, action_11_vector, action_12_vector, action_13_vector, action_14_vector, action_16_vector, action_2, action_26_vector, action_3, action_4, action_5, action_6, action_7, action_8, action_9_vector

- **Debug**: ActivateDebugMenu, LockOnCharacter, SendParamsCamera, SwitchCamera, SwitchGameContext, SwitchGameContextWhilePressed

- **DebugConsole**: Activate, Cancel, Click, Close, Down, Execute, Left, Right, Up

- **DebugDBG**: AabbCheck, AchievementWindow, ActivateAllTasksForCurrentLevel, AddCurrency, AddExperience, AddGameLog, AddTruck, ApplyTruckDamage, CertificationCompatibilityTest, ChangeDamageTarget, CheckAllAddonsAndUpgrades, CheckAllLockedByExplorationCorrect, CheckOneTruckAddonsAndUpgrades, CinematicFlyXZ, CinematicFlyY, CinematicRotate, CinematicSpeedChange, CinematicStartRotate, ClearAndReloadLevel, CollectModelInfo, CompleteCurrentObjective, CoopInfiniteCreateGame, CoopInfiniteFindGame, CopyKsivaToClipboard, CurrencyWindow, DbgInfoWindow, DecreaseQRCodeSize, DisableNetwork, DlcPurchaseWindow, DlcRefundWindow, EmptyFuelTank, EmptyWater, FailCurrentObjective, FillFuelTank, FillWater, FinishOneSubstageInTrackedObjective, FlyCamera, FlyThroughAllRegionsAndLevels, FlyThroughLevel, FlyXZ, FlyY, ForceDaytime, ForceDaytimeSpeed, ForceDelayedCrash5Sec, ForceGarageState, ForceSave, ForceTruckInput, FreeCamera, FreezeTruck, HalfFuelTank, HalfWater, IncreaseQRCodeSize, LevelSelection, LogPhantomsState, LogsTest, MainMenuLobbyForceStart, MakeAllObjectivesAvailable, MinimapTeleportTruckToCursor, ModsCheck, NextLanguage, OpenBannerWindow, OpenCinematicTools, OpenMinimap, OpenNavigationZonesWindow, OpenOriginalTruckWindow, OpenRazerChromaEventsWindow, OpenRestartCompletedObjectiveWindow, OpenTakeTaskWindow, OpenTruckPlacement, OpenTruckPowerGroups, PhantomMode, PreviousLanguage, QuickRepair, ReenterActiveUser, ReloadAllLocalization, ReloadLevelSfx, ReloadSfx, RemoveCurrency, ResetAllObjectives, ResetFiniteCargoInCurrentLevel, RestartObjective, Rotate, SendTelemetry, ShowActiveLayers, ShowAllTutorials, ShowButtons, ShowFriendsList, ShowInputScreenStatus, ShowLeaderboardPopup, ShowMatchmakingState, ShowObjectivesLoca, ShowPresenceList, ShowProfileInfo, ShowProfilesList, ShowProsPopup, ShowUiGfxInputStatus, SkipTime, SpawnSfx, SpeedChange, StartRotate, StartTaskPackUnpackCargo, StartTaskRefuelTank, SwitchCrossSaveView, SwitchHardMode, SwitchNewsView, ToggleChromaInfo, ToggleDebugFpsTable, ToggleDebugInputInfo, ToggleForceSteeringWheelInputPrompts, TraveledDistanceWindow, TruckInputMode, TwoLitersFuelTank, UnfreezeTruck, UnlockAllContent, UnlockAllItem, Watchpoints

- **DriveCamera**: CameraRotationA, CameraRotationD, CameraRotationS, CameraRotationW, GamepadRotation, GamepadZoom, MouseRotation, MouseZoom, RotateStepLeft, RotateStepRight, WheelCameraRotationX, WheelCameraRotationY, WheelCameraZoom

- **Exploration**: Menu, PlayerProfile, SetGearAuto, SetGearHigh, SetGearLow1, SetGearLow2, SetGearLow3, SetGearNeutral, SetGearReverse, ShowMinimap, SkipCinematic, StartCargoManagement, StartMetallodetection, StartSeismicVibration, ToggleHudVisibility, TransmissionDown, TransmissionInput, TransmissionLeft, TransmissionMouseScroll, TransmissionRight, TransmissionUp

- **FlyCamera**: Acceleration, Down, MotionScale, Movement, Rotation, SendParamsCamera, Up

- **Gamepad**: Accept, Back, Enter, GamepadA, GamepadB, GamepadDPadDown, GamepadDPadLeft, GamepadDPadRight, GamepadDPadUp, GamepadLB, GamepadLT, GamepadLeftStickPress, GamepadRB, GamepadRT, GamepadRightStickPress, GamepadX, GamepadY

- **Permanent**: CopyKsiva

- **Popups**: Accept, AcceptBuySellTrailer, AcceptTap, Back, Clear, Close, DecreaseCount, Details, FillWaterStation, IncreaseCount, Load, RefuelRepair, RessuplyWheels, SelectNextMode, SelectPrevMode, ShowMinimap, SwitchCargoLeft, SwitchCargoRight, SwitchPlaceLeft, SwitchPlaceRight, SyncFromCloud, SyncToCloud, tabLeft, tabRight, triggerLeft, triggerRight

- **Truck**: Awd, AwdSolo, CloseFunctionHold, CloseFunctions, ClutchMode, ClutchPedal, DiffLock, DiffLockSolo, Engine, EngineSolo, Functions, Handbrake, HeadlightSolo, Headlights, Honk, MoveTransmission, PullWinch, QuickWinch, ReleaseWinch, RemoveCargo, SelectBottomWinchPoint, SelectLeftWinchPoint, SelectRightWinchPoint, SelectTopWinchPoint, ShowDamageMarkers, SteeringWheel, StopRemoveCargo, ToggleCamera, WheelBrake, WheelThrottle, cinematicMovement, truckMovement

- **UI**: Accept, AcceptContract, AcceptTap, AcceptTutorial, AddonInstall, Back, BlockUnblockPlayer, BuyDlc, CargoStepBtnL, CargoStepBtnR, ClearLastPathPoint, ClearPath, Close, CrossSaveToClient, CrossSaveToCloud, CustomizationStickerSlotNext, CustomizationStickerSlotPrev, DeleteCurrentSlot, DeleteCustomColor, Enter, ExtraTruckPurchase, FakeMovement, FakeRotation, FlyCameraMove, FlyCameraRotate, FlyCameraRotateCW, FlyCameraUpDownMove, FocusSubstage, GamePadChangeScroll, GamepadDPadDown, GamepadDPadLeft, GamepadDPadRight, GamepadDPadUp, GamepadRightStickRotate, GlobalMapGoToGarage, GoToGlobalMap, GoToRegionalMap, HidePolygonTool, HideUi, ImportSave, InviteToParty, KeyboardMove, KeyboardZoom, KickPlayer, LocalMap, MinimapNextTab, MinimapPrevTab, ModBrowserDown, ModBrowserFilter, ModBrowserLeft, ModBrowserMoreOptions, ModBrowserRefresh, ModBrowserRight, ModBrowserScrollDesc, ModBrowserSearch, ModBrowserUp, MouseMove, MouseRotate, MouseWheelDown, MouseWheelPress, MouseWheelUp, MouseZoom, MoveToPlayer, Navigation, NavigationHorizontal, NextPage, OpenCompatiblityPopup, OpenDlcAdvertising, OpenGuides, OpenMOTD, OpenPlatformStore, OpenQRCode, OpenVirtualKeyboard, Party, Paste, PhotoMode, PlacePathMarker, PlayerProfile, PrevPage, RandomAll, ReportPlayer, ResetDefault, RestartContract, RestoreDefault, RestorePurchases, RotateCustomizationCamera, ScrollText, SelectMarker, SelectSettingLeft, SelectSettingRight, ShopScrollLeft, ShopScrollRight, ShowDLC, ShowMOTD, ShowMapDLC, ShowMenu, ShowNGPSettins, ShowPlayerProfile, ShowPros, ShowUserPicker, SkipLoading, SkipTime, SowModsPopup, SwapDescAndStatsVisibility, SwitchDown, SwitchLeft, SwitchRight, SwitchUp, TabBarNextTab, TabBarPrevTab, TabBtnL, TabBtnR, TakePic, Toggle, ToggleMenu, ToggleModTrucksOnly, ToggleSubscribeMod, TruckDeploy, TruckPreview, TruckRetain, TruckSell, UiListDown, UiListDownTap, UiListLeft, UiListLeftTap, UiListRight, UiListRightTap, UiListUp, UiListUpTap, WheelCameraRotationX, WheelCameraRotationY, WheelMinimapMovementX, WheelMinimapMovementY, WinchCheckbox, ZoomCustsomizationCamera

- **UnfocusedMode**: ActivateDeactivate, Click
