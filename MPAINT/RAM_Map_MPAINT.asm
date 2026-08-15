
!RAM_MPAINT_Canvas_CurrentPaletteRowLo = $0000A6
!RAM_MPAINT_Canvas_CurrentPaletteRowHi = !RAM_MPAINT_Canvas_CurrentPaletteRowLo+$001

; $0000B8 - Spray Can Tool Selected

!RAM_MPAINT_Global_HeldButtonsLoP1 = $000132
!RAM_MPAINT_Global_HeldButtonsHiP1 = !RAM_MPAINT_Global_HeldButtonsLoP1+$01
!RAM_MPAINT_Global_HeldButtonsLoP2 = $000134
!RAM_MPAINT_Global_HeldButtonsHiP2 = !RAM_MPAINT_Global_HeldButtonsLoP2+$01

!RAM_MPAINT_Global_PressedButtonsLoP1 = $00013A
!RAM_MPAINT_Global_PressedButtonsHiP1 = !RAM_MPAINT_Global_HeldButtonsLoP1+$01
!RAM_MPAINT_Global_PressedButtonsLoP2 = $00013C
!RAM_MPAINT_Global_PressedButtonsHiP2 = !RAM_MPAINT_Global_HeldButtonsLoP2+$01

!RAM_MPAINT_Global_DisableButtonsLoP1 = $00014A
!RAM_MPAINT_Global_DisableButtonsHiP1 = !RAM_MPAINT_Global_DisableButtonsLoP1+$01
!RAM_MPAINT_Global_DisableButtonsLoP2 = $00014C
!RAM_MPAINT_Global_DisableButtonsHiP2 = !RAM_MPAINT_Global_DisableButtonsLoP2+$01

; $000182 = DMA table

!RAM_MPAINT_Global_OAMBuffer = $000226

!RAM_MPAINT_Global_MouseXDisplacementLo = $0004C6
!RAM_MPAINT_Global_MouseXDisplacementHi = !RAM_MPAINT_Global_MouseXDisplacementLo+$01
!RAM_MPAINT_Global_MouseYDisplacementLo = $0004C8
!RAM_MPAINT_Global_MouseYDisplacementHi = !RAM_MPAINT_Global_MouseYDisplacementLo+$01


!RAM_MPAINT_Global_CursorXPosLo = $0004DC
!RAM_MPAINT_Global_CursorXPosHi = !RAM_MPAINT_Global_CursorXPosLo+$01
!RAM_MPAINT_Global_CursorYPosLo = $0004DE
!RAM_MPAINT_Global_CursorYPosHi = !RAM_MPAINT_Global_CursorYPosLo+$01

!RAM_MPAINT_Global_DemoActiveFlag = $0004E2

; Four 16-byte command queues used by the SNES-side audio bridge.
; CODE_01D308/CODE_01D328/CODE_01D348/CODE_01D368 enqueue one byte.
!RAM_MPAINT_Global_AudioCommandQueue0 = $0004EC
!RAM_MPAINT_Global_AudioCommandQueue1 = $0004FC
!RAM_MPAINT_Global_AudioCommandQueue2 = $00050C
!RAM_MPAINT_Global_AudioCommandQueue3 = $00051C
!RAM_MPAINT_Global_AudioCommandQueueReadIndex0 = $00052C
!RAM_MPAINT_Global_AudioCommandQueueReadIndex1 = $00052D
!RAM_MPAINT_Global_AudioCommandQueueReadIndex2 = $00052E
!RAM_MPAINT_Global_AudioCommandQueueReadIndex3 = $00052F
!RAM_MPAINT_Global_AudioCommandQueueWriteIndex0 = $000530
!RAM_MPAINT_Global_AudioCommandQueueWriteIndex1 = $000531
!RAM_MPAINT_Global_AudioCommandQueueWriteIndex2 = $000532
!RAM_MPAINT_Global_AudioCommandQueueWriteIndex3 = $000533

; $0009A7 = Something related to the clock cursor.

; The music tool stores its complete 0x250-byte working song/settings blob here.
; Pre-composed 0x250-byte songs are copied directly into this range.
!RAM_MPAINT_MusicTool_SongData = $0009E4
!RAM_MPAINT_MusicTool_SongDataEnd = !RAM_MPAINT_MusicTool_SongData+$0250

!RAM_MPAINT_Global_SelectedTilePreviewGFXBuffer_Top = $000F44
!RAM_MPAINT_Global_SelectedTilePreviewGFXBuffer_Bottom = !RAM_MPAINT_Global_SelectedTilePreviewGFXBuffer_Top+$60

!RAM_MPAINT_SpecialStamps_StampGFXBuffer = $00101C

!RAM_MPAINT_Global_CustomStampDisplayGFXBuffer = $001144

!RAM_MPAINT_Canvas_EraseToolSelected = $001992

!RAM_MPAINT_Canvas_EraseToolSize = $001994

; Runtime animation path/settings area. Save code copies all 0x800 bytes into the
; uncompressed save image at $7E9C00 and load code copies them back here.
!RAM_MPAINT_Canvas_AnimationPathAndSettingsBuffer = $7E3800
!RAM_MPAINT_Canvas_AnimationPathAndSettingsBufferEnd = !RAM_MPAINT_Canvas_AnimationPathAndSettingsBuffer+$0800

!RAM_MPAINT_Canvas_AnimationCellGFXBuffer = $7E4000
!RAM_MPAINT_Canvas_CanvasGFXBuffer = $7EA000

; Uncompressed composition image consumed by CODE_01EDDB.
; These boundaries are based on direct save/load copies in bank $00 and the
; fixed $BA52-byte source length in the compression routine.
!RAM_MPAINT_SaveImage_Base = $7E4400
!RAM_MPAINT_SaveImage_AnimationRegion = $7E4400
!RAM_MPAINT_SaveImage_AnimationPathAndSettings = $7E9C00
!RAM_MPAINT_SaveImage_CanvasRegion = $7EA400
!RAM_MPAINT_SaveImage_MusicToolData = $7EFC00
!RAM_MPAINT_SaveImage_MusicToolDataEnd = !RAM_MPAINT_SaveImage_MusicToolData+$0250
!RAM_MPAINT_SaveImage_UnidentifiedTail = $7EFE50
!RAM_MPAINT_SaveImage_End = $7EFE52

!RAM_MPAINT_TitleScreen_CreditsLineIndex = $7F020D

!RAM_MPAINT_TitleScreen_WaitBeforeDisplayingNextCreditsLine = $7F020F

!RAM_MPAINT_TitleScreen_WaitBeforeStartingDemo = $7F0411

struct MPAINT_Global_OAMBuffer !RAM_MPAINT_Global_OAMBuffer
	.XDisp: skip $01
	.YDisp: skip $01
	.Tile: skip $01
	.Prop: skip $01
endstruct align $04

struct MPAINT_Global_UpperOAMBuffer !RAM_MPAINT_Global_OAMBuffer+$0200
	.Slot: skip $01
endstruct align $01
