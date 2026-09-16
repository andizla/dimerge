# SnowRunner keybinds on the merged wheel

Generated 2026-09-13 by tools/keybinds/keybinds.py from initial.pak, user_settings.cfg and bin/dimerge.ini. Re-run it after changing bindings or the ini.

## Merged key numbers

The game stores a wheel binding as a key number: buttons 0 to 127, axes 128 to 135 (X, Y, Z, Rx, Ry, Rz, Slider0, Slider1), hat directions 136 to 139 (up, right, down, left). What the proxy puts on each number:

| Number | Physical control |
|---|---|
| 64 to 89 | PXN-CB1 buttons 0 to 25 |
| 90 | PXN-CB1 joystick up |
| 91 | PXN-CB1 joystick right |
| 92 | PXN-CB1 joystick down |
| 93 | PXN-CB1 joystick left |
| 96 | GX100 gate 1 |
| 97 | GX100 gate 2 |
| 98 | GX100 gate 3 |
| 99 | GX100 gate 4 |
| 100 | GX100 gate 5 |
| 101 | GX100 gate 6 |
| 102 | GX100 firmware gear 7 (Comb switch on only) |
| 103 | GX100 firmware gear 8 (Comb switch on only) |
| 104 | GX100 sequential down |
| 105 | GX100 sequential up |
| 106 | GX100 firmware reverse (Comb switch on only) |
| 110 | GX100 pull collar (hidden from the game: ShiftHide=1) |
| 112 | GX100 collar + gate 1 |
| 113 | GX100 collar + gate 2 |
| 114 | GX100 collar + gate 3 |
| 115 | GX100 collar + gate 4 |
| 116 | GX100 collar + gate 5 |
| 117 | GX100 collar + gate 6 |
| 128 | wheelbase 0483:0522 axis X |
| 129 | brake pedal (Y) |
| 133 | clutch pedal (Rz) |
| 134 | throttle pedal (Slider0) |
| 136 | wheelbase 0483:0522 hat up |
| 137 | wheelbase 0483:0522 hat right |
| 138 | wheelbase 0483:0522 hat down |
| 139 | wheelbase 0483:0522 hat left |
| other 0 to 63 | wheelbase 0483:0522 buttons, native |

## Binding slots of a custom wheel

Every slot the settings menu offers for a wheel the game does not know by vendor id. Slots marked added come from tools/pakpatch/crane_slots.py; the stock game has no wheel slot for moving the crane or entering crane mode.

| Slot | Menu name | Game input | Context | Bound now |
|---|---|---|---|---|
| SteeringWheel | Steering Wheel Axis | Truck.SteeringWheel | GAME | 128 = wheelbase 0483:0522 axis X |
| Throttle | Throttle Pedal | Truck.WheelThrottle | GAME | 134 = throttle pedal (Slider0) |
| Brake | Brake | Truck.WheelBrake | GAME | 129 = brake pedal (Y) |
| Clutch | Clutch Pedal | Truck.ClutchPedal | GAME | 133 = clutch pedal (Rz) |
| Headlights | Headlight | Truck.HeadlightSolo | GAME | 68 = PXN-CB1 button 4 |
| Honk | Honk | Truck.Honk | GAME | 64 = PXN-CB1 button 0 |
| AWD | AWD | Truck.AwdSolo | GAME |  |
| DiffLock | Diff. Lock | Truck.DiffLockSolo | GAME |  |
| Handbrake | Handbrake | Truck.Handbrake | GAME | 75 = PXN-CB1 button 11 |
| QuickWinch | Quick Winch | Truck.QuickWinch, Truck.PullWinch | GAME | 69 = PXN-CB1 button 5 |
| StartEngine | Engine | Truck.EngineSolo | GAME | 74 = PXN-CB1 button 10 |
| Minimap | Navigation Map | Exploration.ShowMinimap | GAME | 89 = PXN-CB1 button 25 |
| FunctionMenu | Functions | Truck.Functions | GAME | 88 = PXN-CB1 button 24 |
| IngameMenu | Pause Menu | UI.ToggleMenu, Exploration.Menu, UI.ShowMenu | UI |  |
| OpenPlayerProfile | Player's Profile | Exploration.PlayerProfile | GAME |  |
| DpadUp | Up | UI.GamepadDPadUp, Gamepad.GamepadDPadUp, Exploration.TransmissionUp, Truck.SelectTopWinchPoint, Exploration.TransmissionInput | FUNCTION_MENU | 90 = PXN-CB1 joystick up |
| DpadDown | Down | UI.GamepadDPadDown, Gamepad.GamepadDPadDown, Exploration.TransmissionDown, Truck.SelectBottomWinchPoint, Exploration.TransmissionInput backward | FUNCTION_MENU | 92 = PXN-CB1 joystick down |
| DpadLeft | Left | UI.GamepadDPadLeft, Gamepad.GamepadDPadLeft, Exploration.TransmissionLeft, Truck.SelectLeftWinchPoint, Exploration.TransmissionInput left | TUTORIAL | 93 = PXN-CB1 joystick left |
| DpadRight | Right | UI.GamepadDPadRight, Gamepad.GamepadDPadRight, Exploration.TransmissionRight, Truck.SelectRightWinchPoint, Exploration.TransmissionInput right | TUTORIAL | 91 = PXN-CB1 joystick right |
| UIAccept | Accept | UI.Accept, Gamepad.GamepadA, Popups.Accept, Popups.Load, Popups.RefuelRepair, Popups.FillWaterStation, Exploration.SkipCinematic, UI.Enter, UI.Accept, UI.AcceptTap, UI.TruckDeploy, UI.FocusSubstage, UI.InviteToParty, UI.ShowPlayerProfile, Gamepad.Enter, Gamepad.Accept, Popups.AcceptTap, Popups.AcceptBuySellTrailer, Truck.RemoveCargo | FUNCTION_MENU | 73 = PXN-CB1 button 9 |
| UIDecline | Decline | UI.Back, Gamepad.Back, Popups.Back, Truck.ReleaseWinch, Popups.Close, Truck.CloseFunctions, Truck.StopRemoveCargo | CUSTOM_ADDON_ACTION | 72 = PXN-CB1 button 8 |
| UITabLeft | UI Left | UI.TabBarPrevTab, UI.MinimapPrevTab, UI.TabBtnL, UI.OpenGuides, UI.PrevPage, Popups.tabLeft, Popups.SwitchCargoLeft, UI.SelectSettingLeft | TUTORIAL |  |
| UITabRight | UI Right | UI.TabBarNextTab, UI.MinimapNextTab, UI.TabBtnR, UI.NextPage, Popups.tabRight, Popups.SwitchCargoRight, UI.SelectSettingRight | TUTORIAL |  |
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
| CraneWinchLift | Lift Crane Winch | Crane.WinchActions | CRANE | 78 = PXN-CB1 button 14 |
| CraneWinchLower | Lower Crane Winch | Crane.WinchActions backward | CRANE | 79 = PXN-CB1 button 15 |
| CraneWinchRotateLeft | Pull Crane Winch to the left | Crane.WinchActions left | CRANE | 80 = PXN-CB1 button 16 |
| CraneWinchRotateRight | Pull Crane Winch to the right | Crane.WinchActions right | CRANE | 81 = PXN-CB1 button 17 |
| CraneArrowLift | Lift Crane Arrow | Crane.moveYPlane | CRANE | 82 = PXN-CB1 button 18 |
| CraneArrowLower | Lower Crane Arrow | Crane.moveYPlane backward | CRANE | 83 = PXN-CB1 button 19 |
| CraneMoveForward (added) | Move Crane Forward | Crane.moveXZplane | CRANE | 90 = PXN-CB1 joystick up |
| CraneMoveBackward (added) | Move Crane Backward | Crane.moveXZplane backward | CRANE | 92 = PXN-CB1 joystick down |
| CraneMoveLeft (added) | Move Crane Left | Crane.moveXZplane left | CRANE | 93 = PXN-CB1 joystick left |
| CraneMoveRight (added) | Move Crane Right | Crane.moveXZplane right | CRANE | 91 = PXN-CB1 joystick right |
| CraneTurnOn (added) | Enter Crane Mode | Crane.TurnOn | GAME | 71 = PXN-CB1 button 7 |
| CraneAttachCargo (added) | Attach or Detach Cargo | Crane.AttachCargo | CRANE | 76 = PXN-CB1 button 12 |
| CraneAnchor (added) | Crane Anchor | Crane.EnableAnchor | CRANE | 77 = PXN-CB1 button 13 |
| GarageGlobalMap | Move to Global Map from Garage | UI.GoToGlobalMap | GARAGE_SLOT_MENU |  |
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

All Context.Action names the game data mentions (HUD hints, presets, menus). A wheel slot targets one of these; anything without a slot above is keyboard or gamepad only for a custom wheel unless a slot is added the way crane_slots.py does it.

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
