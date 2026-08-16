; Research map for the 0x250-byte Mario Paint Music Tool working blob.
; This file is intentionally not included by the ROM build yet.

!MPAINT_MusicTool_BlobSize = $0250
!MPAINT_MusicTool_EventDataSize = $0240
!MPAINT_MusicTool_MaxSteps = 96
!MPAINT_MusicTool_SlotsPerStep = 3
!MPAINT_MusicTool_EventSize = 2

!RAM_MPAINT_MusicTool_EventData = $0009E4
!RAM_MPAINT_MusicTool_SongEnd = $000C24
!RAM_MPAINT_MusicTool_Loop = $000C26
!RAM_MPAINT_MusicTool_TempoRaw = $000C28
!RAM_MPAINT_MusicTool_TempoIncrementLo = $000C2A
!RAM_MPAINT_MusicTool_TempoIncrementHi = $000C2C
!RAM_MPAINT_MusicTool_PlaybackPhaseLo = $000C2E
!RAM_MPAINT_MusicTool_PlaybackPhaseHi = $000C30
!RAM_MPAINT_MusicTool_Meter = $000C32
!RAM_MPAINT_MusicTool_BlobEnd = $000C34

!MPAINT_MusicTool_InactiveEventBit = $8000
!MPAINT_MusicTool_CursorHighlightBit = $2000

; DSP voice assignment used by the three Music Tool event slots (zero-based).
!MPAINT_MusicTool_Slot0SPCVoice = 5
!MPAINT_MusicTool_Slot1SPCVoice = 6
!MPAINT_MusicTool_Slot2SPCVoice = 7
