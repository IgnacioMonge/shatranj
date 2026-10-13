; Journal copy of the SAVELOAD wrappers: TMP is created read/write so SAVE
; can reread it through the persistent handle. CONFIG keeps esx_saveload.asm.
defc ESX_SAVELOAD = 1
defc ESX_JOURNAL = 1
INCLUDE "asm/esxdos/esx_fileio_spectalk.asm"
