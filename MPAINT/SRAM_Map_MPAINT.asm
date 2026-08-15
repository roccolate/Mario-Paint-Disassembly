
!SRAM_MPAINT_Global_SavedSpecialStampsGFX = $000000+!SRAMBankBaseAddress

; Save/load metadata immediately before the compressed composition payload.
; The save routine initializes the additive checksum with $7003 and the XOR
; checksum with $2122, folds every payload word into them, then folds in the
; meaningful Huffman payload size before storing these values.
!SRAM_MPAINT_Save_PayloadAddChecksum = $0007C2+!SRAMBankBaseAddress
!SRAM_MPAINT_Save_PayloadXorChecksum = $0007C4+!SRAMBankBaseAddress
!SRAM_MPAINT_Save_HuffmanPayloadSize = $0007FE+!SRAMBankBaseAddress

; The composition save payload occupies a fixed 0x7800-byte SRAM range.
!SRAM_MPAINT_Save_CompressedPayload = $000800+!SRAMBankBaseAddress
!SRAM_MPAINT_Save_CompressedPayloadEnd = $008000+!SRAMBankBaseAddress
