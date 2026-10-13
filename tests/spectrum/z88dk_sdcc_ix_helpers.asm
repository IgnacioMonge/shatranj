; Copies of z88dk libsrc/l/sdcc/___sdcc_enter_ix.asm and
; ____sdcc_4_push_hlix.asm (z88dk revision 762578732c, Clarified Artistic
; License 2.0) for vector builds on z88dk installs that ship without libsrc,
; such as the pinned CI image. Installed sources take precedence.


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


SECTION code_clib
SECTION code_l_sdcc

PUBLIC ____sdcc_4_push_hlix

____sdcc_4_push_hlix:

   pop af
   push af
   push af
   push af
   
   push de
   
IFDEF __SDCC_IX

   push ix
   pop de

ELSE

   push iy
   pop de
   
ENDIF

   add hl,de
   
   ex de,hl
   
   ld hl,2+2
   add hl,sp
   
   ex de,hl
   
   ldi
   ldi
   ldi
   ld a,(hl)
   ld (de),a
   
   inc bc
   inc bc
   inc bc
   
   pop de
   ret
