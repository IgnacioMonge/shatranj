; Copy of z88dk libsrc/l/sdcc/___sdcc_enter_ix.asm (revision 762578732c, Clarified Artistic
; License 2.0) for vector builds on z88dk installs without libsrc.


SECTION code_clib
SECTION code_l_sdcc

PUBLIC ___sdcc_enter_ix

___sdcc_enter_ix:

IF __SDCC_IX

   ex (sp),ix
	push ix
	
	ld ix,2
	add ix,sp
	
	ret

ELSE

   ex (sp),iy
	push iy
	
	ld iy,2
	add iy,sp
	
	ret

ENDIF
