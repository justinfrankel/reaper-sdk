BITS 32

extern _pnts

%macro	step1 1
	fld dword [edx+4*(%1)]
	fld dword [edx+4*(0x1F-(%1))]
	fld st1
	fadd st0,st1
	fstp dword [ecx+4*(%1)]
	fsubp st1,st0
	fmul dword [eax+4*(%1)]
	fstp dword [ecx+4*(0x1F-(%1))]
%endmacro

%macro	step2_1 1
	fld dword [ecx+4*(%1)]
	fld dword [ecx+4*(0x0F-(%1))]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)]
	fsubp st1,st0
	fmul dword [eax+4*(%1)]
	fstp dword [edx+4*(0x0F-(%1))]
%endmacro

%macro	step2_2 1
	fld dword [ecx+4*(%1)]
	fld dword [ecx+4*(0x0F-(%1))]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)]
	fsubrp st1,st0
	fmul dword [eax+4*(%1)]
	fstp dword [edx+4*(0x0F-(%1))]
%endmacro

%macro	step3_1 1
	fld dword [edx+4*(%1)]
	fld dword [edx+4*(0x07-(%1))]
	fld st1
	fadd st0,st1
	fstp dword [ecx+4*(%1)]
	fsubp st1,st0
	fmul dword [eax+4*(%1)]
	fstp dword [ecx+4*(0x07-(%1))]
%endmacro

%macro	step3_2 1
	fld dword [edx+4*(%1)]
	fld dword [edx+4*(0x07-(%1))]
	fld st1
	fadd st0,st1
	fstp dword [ecx+4*(%1)]
	fsubrp st1,st0
	fmul dword [eax+4*(%1)]
	fstp dword [ecx+4*(0x07-(%1))]
%endmacro

%macro	step4_1 1
	fld dword [ecx+4*(%1)]
	fld dword [ecx+4*(%1)+4*3]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)]
	fsubp st1,st0
	fmul st0,st1
	fstp dword [edx+4*(%1)+4*3]
	fld dword [ecx+4*(%1)+4*1]
	fld dword [ecx+4*(%1)+4*2]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)+4*1]
	fsubp st1,st0
	fmul st0,st2
	fstp dword [edx+4*(%1)+4*2]
%endmacro

%macro	step4_2 1
	fld dword [ecx+4*(%1)]
	fld dword [ecx+4*(%1)+4*3]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)]
	fsubrp st1,st0
	fmul st0,st1
	fstp dword [edx+4*(%1)+4*3]
	fld dword [ecx+4*(%1)+4*1]
	fld dword [ecx+4*(%1)+4*2]
	fld st1
	fadd st0,st1
	fstp dword [edx+4*(%1)+4*1]
	fsubrp st1,st0
	fmul st0,st2
	fstp dword [edx+4*(%1)+4*2]
%endmacro

%macro	step5_1 1
	fld dword [edx+4*(%1)+4*0]
	fld dword [edx+4*(%1)+4*1]
	fld st1
	fadd st0,st1
	fstp dword [ecx+4*(%1)+4*0]
	fsubp st1,st0
	fmul st0,st1
	fstp dword [ecx+4*(%1)+4*1]

	fld dword [edx+4*(%1)+4*2]
	fld dword [edx+4*(%1)+4*3]
	fld st1
	fadd st0,st1
	fxch st0,st2
	fsubp st1,st0
	fmul st0,st2
	fst dword [ecx+4*(%1)+4*3]
	faddp st1,st0
	fstp dword [ecx+4*(%1)+4*2]
%endmacro


%macro	step5_2 1
	fld dword [edx+4*(%1)+4*0]
	fld dword [edx+4*(%1)+4*1]
	fld st1
	fadd st0,st1
	fstp dword [ecx+4*(%1)+4*0]
	fsubp st1,st0
	fmul st0,st1
	fstp dword [ecx+4*(%1)+4*1]

	fld dword [edx+4*(%1)+4*2]
	fld dword [edx+4*(%1)+4*3]
	fld st1
	fadd st0,st1
	fxch st0,st2
	fsubp st1,st0
	fmul st0,st2
	fst dword [ecx+4*(%1)+4*3]	; optimizeme
	fxch st0,st1
	fadd st0,st1				;b1[2]+=b1[3]
	fst dword [ecx+4*(%1)+4*2]
	fadd dword [ecx+4*(%1)+4*0] ;b1[0]+=b1[2]
	fstp dword [ecx+4*(%1)+4*0]
	fld dword [ecx+4*(%1)+4*1]  ;b1[2]+=b1[1]
	fld st0
	fadd dword [ecx+4*(%1)+4*2]
	fstp dword [ecx+4*(%1)+4*2]
	faddp st1,st0
	fstp dword [ecx+4*(%1)+4*1]
%endmacro


segment .text
align 32

; void dct64_asm_x87(real *out0,real *out1,real *b1,real *b2,real *samples);

global _dct64_asm_x87
_dct64_asm_x87:
	;out0: [esp+4], out1: [esp+8], b1: [esp+12], b2: [esp+16], samples: [esp+20]
	push ebx
	push edi
	push esi
	mov eax,dword [_pnts] ; costab
	mov ecx, dword [esp+12+12] ; b1
	mov edx, dword [esp+12+20] ; samples
	mov esi, [esp+12+4];out0
	mov edi, [esp+12+8];out1
	step1 0x00
	step1 0x01
	step1 0x02
	step1 0x03
	step1 0x04
	step1 0x05
	step1 0x06
	step1 0x07
	step1 0x08
	step1 0x09
	step1 0x0A
	step1 0x0B
	step1 0x0C
	step1 0x0D
	step1 0x0E
	step1 0x0F
	mov eax,dword [_pnts+4] ; costab
	;mov ecx, dword ptr [esp+12+12] ; b1
	mov edx,dword [esp+12+16] ; b2
	step2_1 0x00
	step2_1 0x01
	step2_1 0x02
	step2_1 0x03
	step2_1 0x04
	step2_1 0x05
	step2_1 0x06
	step2_1 0x07
	add ecx,0x10*4
	add edx,0x10*4
	step2_2 0x00
	step2_2 0x01
	step2_2 0x02
	step2_2 0x03
	step2_2 0x04
	step2_2 0x05
	step2_2 0x06
	step2_2 0x07
	mov eax,dword [_pnts+8] ; costab
	sub ecx,0x10*4;b1
	sub edx,0x10*4;b2
	step3_1 0x00
	step3_1 0x01
	step3_1 0x02
	step3_1 0x03
	add ecx,0x08*4
	add edx,0x08*4
	step3_2 0x00
	step3_2 0x01
	step3_2 0x02
	step3_2 0x03
	add ecx,0x08*4
	add edx,0x08*4
	step3_1 0x00
	step3_1 0x01
	step3_1 0x02
	step3_1 0x03
	add ecx,0x08*4
	add edx,0x08*4
	step3_2 0x00
	step3_2 0x01
	step3_2 0x02
	step3_2 0x03
	mov eax, dword [_pnts + 12]
	sub ecx,0x18*4;b1
	sub edx,0x18*4;b2
	fld dword [eax+4]
	fld dword [eax]
	step4_1 0x00
	step4_2 0x04
	step4_1 0x08
	step4_2 0x0C
	step4_1 0x10
	step4_2 0x14
	step4_1 0x18
	step4_2 0x1C
	fstp st0
	fstp st0
	mov eax, dword [_pnts + 16]
	fld dword [eax]
	step5_1 0x00
	step5_2 0x04
	step5_1 0x08
	step5_2 0x0C
	step5_1 0x10
	step5_2 0x14
	step5_1 0x18
	step5_2 0x1C
	fstp st0
	mov eax,[ecx+0*4]	;b1[0x00]
	mov ebx,[ecx+4*4]	;b1[0x04]
	mov [esi+4*0x10*16],eax
	mov [esi+4*0x10*12],ebx
	mov eax,[ecx+2*4]	;b1[0x02]
	mov ebx,[ecx+6*4]	;b1[0x06]
	mov [esi+4*0x10*8],eax
	mov [esi+4*0x10*4],ebx
	mov eax,[ecx+1*4]	;b1[0x01]
	mov ebx,[ecx+5*4]	;b1[0x05]
	mov [esi+4*0x10*0],eax
	mov [edi+4*0x10*0],eax	
	mov [edi+4*0x10*4],ebx
	mov eax,[ecx+3*4]	;b1[0x03]
	mov ebx,[ecx+7*4]	;b1[0x07]
	mov [edi+4*0x10*8],eax
	mov [edi+4*0x10*12],ebx

	fld dword [ecx+4*0x0C]
	fld st0
	fadd dword [ecx+4*0x08]
	fst dword [ecx+4*0x08]
	fstp dword [esi+4*0x10*14]
	
	fld dword [ecx+4*0x0A]
	fadd st1,st0
	fxch st0,st1	
	fst dword [ecx+4*0x0C]
	fstp dword [esi+4*0x10*10]

	fld dword [ecx+4*0x0E]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0A]
	fstp dword [esi+4*0x10*6]

	fld dword [ecx+4*0x09]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0E]
	fstp dword [esi+4*0x10*2]

	fld dword [ecx+4*0x0D]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x09]
	fstp dword [edi+4*0x10*2]
	
	fld dword [ecx+4*0x0B]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0D]
	fstp dword [edi+4*0x10*6]

	fld dword [ecx+4*0x0F]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0B]
	fstp dword [edi+4*0x10*10]

	fstp dword [edi+4*0x10*14]


	fld dword [ecx+4*0x18]
	fadd dword [ecx+4*0x1C]
	;fst dword [ecx+4*0x18]
	fld st0
	fadd dword [ecx+4*0x10]
	fstp dword [esi+4*0x10*15]
	fadd dword [ecx+4*0x14]
	fstp dword [esi+4*0x10*13]

	fld dword [ecx+4*0x1C]
	fadd dword [ecx+4*0x1A]
	;fst dword [ecx+4*0x1C]
	fld st0
	fadd dword [ecx+4*0x14]
	fstp dword [esi+4*0x10*11]
	fadd dword [ecx+4*0x12]
	fstp dword [esi+4*0x10*9]

	fld dword [ecx+4*0x1A]
	fadd dword [ecx+4*0x1E]
	;fst dword [ecx+4*0x1A]
	fld st0
	fadd dword [ecx+4*0x12]
	fstp dword [esi+4*0x10*7]
	fadd dword [ecx+4*0x16]
	fstp dword [esi+4*0x10*5]

	fld dword [ecx+4*0x1E]
	fadd dword [ecx+4*0x19]
	;fst dword [ecx+4*0x1E]
	fld st0
	fadd dword [ecx+4*0x16]
	fstp dword [esi+4*0x10*3]
	fadd dword [ecx+4*0x11]
	fstp dword [esi+4*0x10*1]

	fld dword [ecx+4*0x19]
	fadd dword [ecx+4*0x1D]
	;fst dword [ecx+4*0x19]
	fld st0
	fadd dword [ecx+4*0x11]
	fstp dword [edi+4*0x10*1]
	fadd dword [ecx+4*0x15]
	fstp dword [edi+4*0x10*3]
	
	fld dword [ecx+4*0x1D]
	fadd dword [ecx+4*0x1B]
	;fst dword [ecx+4*0x1D]
	fld st0
	fadd dword [ecx+4*0x15]
	fstp dword [edi+4*0x10*5]
	fadd dword [ecx+4*0x13]
	fstp dword [edi+4*0x10*7]
	
	fld dword [ecx+4*0x1B]
	fadd dword [ecx+4*0x1F]
	;fst dword [ecx+4*0x1B]
	fld st0
	fadd dword [ecx+4*0x13]
	fstp dword [edi+4*0x10*9]
	fadd dword [ecx+4*0x17]
	fstp dword [edi+4*0x10*11]
	
	fld dword [ecx+4*0x1F]
	fld st0
	fadd dword [ecx+4*0x17]
	fstp dword [edi+4*0x10*13]
	fstp dword [edi+4*0x10*15]

	pop esi
	pop edi
	pop ebx
	ret


%macro	step1_3dnow 1
	movq mm0,qword [edx+4*(%1)]
	pswapd mm1,qword [edx+4*(0x1E-(%1))]
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,qword [eax + 4*(%1)]
	movq qword [ecx+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [ecx+4*(0x1E-(%1))],mm0
%endmacro


%macro	step2_1_3dnow 1
	movq mm0,qword [ecx+4*(%1)]
	pswapd mm1,qword [ecx+4*(0x0E-(%1))]
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,qword [eax + 4*(%1)]
	movq qword [edx+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [edx+4*(0x0E-(%1))],mm0
%endmacro

%macro	step2_2_3dnow 1
	movq mm0,qword [ecx+4*0x10+4*(%1)]
	pswapd mm1,qword [ecx+4*0x10+4*(0x0E-(%1))]
	movq mm2,mm0
	pfsubr mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,qword [eax + 4*(%1)]
	movq qword [edx+4*0x10+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [edx+4*0x10+4*(0x0E-(%1))],mm0
%endmacro

%macro	step3_1_3dnow 2
	movq mm0,qword [edx+4*(%2)+4*(%1)]
	pswapd mm1,qword [edx+4*(%2)+4*(0x06-(%1))]
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,qword [eax + 4*(%1)]
	movq qword [ecx+4*(%2)+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [ecx+4*(%2)+4*(0x06-(%1))],mm0
%endmacro

%macro	step3_2_3dnow 2
	movq mm0,qword [edx+4*(%2)+4*(%1)]
	pswapd mm1,qword [edx+4*(%2)+4*(0x06-(%1))]
	movq mm2,mm0
	pfsubr mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,qword [eax + 4*(%1)]
	movq qword [ecx+4*(%2)+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [ecx+4*(%2)+4*(0x06-(%1))],mm0
%endmacro

%macro	step4_1_3dnow 2
	movq mm0,qword [ecx+4*(%2)+4*(%1)]
	pswapd mm1,qword [ecx+4*(%2)+4*(0x02-(%1))]
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,mm7
	movq qword [edx+4*(%2)+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [edx+4*(%2)+4*(0x02-(%1))],mm0
%endmacro

%macro	step4_2_3dnow 2
	movq mm0,qword [ecx+4*(%2)+4*(%1)]
	pswapd mm1,qword [ecx+4*(%2)+4*(0x02-(%1))]
	movq mm2,mm0
	pfsubr mm0,mm1
	pfadd mm2,mm1
	pfmul mm0,mm7
	movq qword [edx+4*(%2)+4*(%1)],mm2
	pswapd mm0,mm0
	movq qword [edx+4*(%2)+4*(0x02-(%1))],mm0
%endmacro


align 32

global _dct64_asm_3dnow
_dct64_asm_3dnow:
	;out0: [esp+4], out1: [esp+8], b1: [esp+12], b2: [esp+16], samples: [esp+20]
	push ebx
	push edi
	push esi
	mov eax,dword [_pnts] ; costab
	mov ecx, dword [esp+12+12] ; b1
	mov edx, dword [esp+12+20] ; samples
	mov esi, [esp+12+4];out0
	mov edi, [esp+12+8];out1
	step1_3dnow 0x00
	step1_3dnow 0x02
	step1_3dnow 0x04
	step1_3dnow 0x06
	step1_3dnow 0x08
	step1_3dnow 0x0A
	step1_3dnow 0x0C
	step1_3dnow 0x0E
	mov eax,dword [_pnts+4] ; costab
	;mov ecx, dword ptr [esp+12+12] ; b1
	mov edx,dword [esp+12+16] ; b2
	step2_1_3dnow 0x00
	step2_1_3dnow 0x02
	step2_1_3dnow 0x04
	step2_1_3dnow 0x06
	step2_2_3dnow 0x00
	step2_2_3dnow 0x02
	step2_2_3dnow 0x04
	step2_2_3dnow 0x06
	mov eax,dword [_pnts+8] ; costab
	step3_1_3dnow 0x00,0x00
	step3_1_3dnow 0x02,0x00
	step3_2_3dnow 0x00,0x08
	step3_2_3dnow 0x02,0x08
	step3_1_3dnow 0x00,0x10
	step3_1_3dnow 0x02,0x10
	step3_2_3dnow 0x00,0x18
	step3_2_3dnow 0x02,0x18
	mov eax, dword [_pnts + 12]
	movq mm7,qword [eax]
	step4_1_3dnow 0x00,0x00
	step4_2_3dnow 0x00,0x04
	step4_1_3dnow 0x00,0x08
	step4_2_3dnow 0x00,0x0C
	step4_1_3dnow 0x00,0x10
	step4_2_3dnow 0x00,0x14
	step4_1_3dnow 0x00,0x18
	step4_2_3dnow 0x00,0x1C
	femms
	mov eax, dword [_pnts + 16]
	fld dword [eax]
	step5_1 0x00
	step5_2 0x04
	step5_1 0x08
	step5_2 0x0C
	step5_1 0x10
	step5_2 0x14
	step5_1 0x18
	step5_2 0x1C
	fstp st0
	mov eax,[ecx+0*4]	;b1[0x00]
	mov ebx,[ecx+4*4]	;b1[0x04]
	mov [esi+4*0x10*16],eax
	mov [esi+4*0x10*12],ebx
	mov eax,[ecx+2*4]	;b1[0x02]
	mov ebx,[ecx+6*4]	;b1[0x06]
	mov [esi+4*0x10*8],eax
	mov [esi+4*0x10*4],ebx
	mov eax,[ecx+1*4]	;b1[0x01]
	mov ebx,[ecx+5*4]	;b1[0x05]
	mov [esi+4*0x10*0],eax
	mov [edi+4*0x10*0],eax	
	mov [edi+4*0x10*4],ebx
	mov eax,[ecx+3*4]	;b1[0x03]
	mov ebx,[ecx+7*4]	;b1[0x07]
	mov [edi+4*0x10*8],eax
	mov [edi+4*0x10*12],ebx

	fld dword [ecx+4*0x0C]
	fld st0
	fadd dword [ecx+4*0x08]
	fst dword [ecx+4*0x08]
	fstp dword [esi+4*0x10*14]
	
	fld dword [ecx+4*0x0A]
	fadd st1,st0
	fxch st0,st1	
	fst dword [ecx+4*0x0C]
	fstp dword [esi+4*0x10*10]

	fld dword [ecx+4*0x0E]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0A]
	fstp dword [esi+4*0x10*6]

	fld dword [ecx+4*0x09]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0E]
	fstp dword [esi+4*0x10*2]

	fld dword [ecx+4*0x0D]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x09]
	fstp dword [edi+4*0x10*2]
	
	fld dword [ecx+4*0x0B]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0D]
	fstp dword [edi+4*0x10*6]

	fld dword [ecx+4*0x0F]
	fadd st1,st0
	fxch st0,st1
	fst dword [ecx+4*0x0B]
	fstp dword [edi+4*0x10*10]

	fstp dword [edi+4*0x10*14]

	fld dword [ecx+4*0x18]
	fadd dword [ecx+4*0x1C]
	;fst dword [ecx+4*0x18]
	fld st0
	fadd dword [ecx+4*0x10]
	fstp dword [esi+4*0x10*15]
	fadd dword [ecx+4*0x14]
	fstp dword [esi+4*0x10*13]

	fld dword [ecx+4*0x1C]
	fadd dword [ecx+4*0x1A]
	;fst dword [ecx+4*0x1C]
	fld st0
	fadd dword [ecx+4*0x14]
	fstp dword [esi+4*0x10*11]
	fadd dword [ecx+4*0x12]
	fstp dword [esi+4*0x10*9]

	fld dword [ecx+4*0x1A]
	fadd dword [ecx+4*0x1E]
	;fst dword [ecx+4*0x1A]
	fld st0
	fadd dword [ecx+4*0x12]
	fstp dword [esi+4*0x10*7]
	fadd dword [ecx+4*0x16]
	fstp dword [esi+4*0x10*5]

	fld dword [ecx+4*0x1E]
	fadd dword [ecx+4*0x19]
	;fst dword [ecx+4*0x1E]
	fld st0
	fadd dword [ecx+4*0x16]
	fstp dword [esi+4*0x10*3]
	fadd dword [ecx+4*0x11]
	fstp dword [esi+4*0x10*1]

	fld dword [ecx+4*0x19]
	fadd dword [ecx+4*0x1D]
	;fst dword [ecx+4*0x19]
	fld st0
	fadd dword [ecx+4*0x11]
	fstp dword [edi+4*0x10*1]
	fadd dword [ecx+4*0x15]
	fstp dword [edi+4*0x10*3]
	
	fld dword [ecx+4*0x1D]
	fadd dword [ecx+4*0x1B]
	;fst dword [ecx+4*0x1D]
	fld st0
	fadd dword [ecx+4*0x15]
	fstp dword [edi+4*0x10*5]
	fadd dword [ecx+4*0x13]
	fstp dword [edi+4*0x10*7]
	
	fld dword [ecx+4*0x1B]
	fadd dword [ecx+4*0x1F]
	;fst dword [ecx+4*0x1B]
	fld st0
	fadd dword [ecx+4*0x13]
	fstp dword [edi+4*0x10*9]
	fadd dword [ecx+4*0x17]
	fstp dword [edi+4*0x10*11]
	
	fld dword [ecx+4*0x1F]
	fld st0
	fadd dword [ecx+4*0x17]
	fstp dword [edi+4*0x10*13]
	fstp dword [edi+4*0x10*15]

	pop esi
	pop edi
	pop ebx
	ret


global _detect_3dnow,_detect_3dnow_ex;

testCPUID:
        pushfd
        pop     eax
        mov     ecx,eax
        xor     eax,0x200000
        push    eax
        popfd
        pushfd
        pop     eax
        cmp     eax,ecx
        mov     eax,1
        ret

return0:
		popad
		xor eax,eax
		ret
return1:
		popad
		mov eax,1
		ret

_detect_3dnow:
		
        pushad
        call    testCPUID
        jz      return0         ; no CPUID command, so no 3DNow!

        mov     eax,0x80000000
        CPUID
        cmp     eax,0x80000000
        jbe     return0         ; no extended MSR(1), so no 3DNow!

        mov     eax,0x80000001
        CPUID
        test    edx,0x80000000
        jz      return0
        jmp     return1


_detect_3dnow_ex:
        pushad
        call    testCPUID
        jz      return0         ; no CPUID command, so no 3DNow!

        mov     eax,0x80000000
        CPUID
        cmp     eax,0x80000000
        jbe     return0         ; no extended MSR(1), so no 3DNow!

        mov     eax,0x80000001
        CPUID
        test    edx,0x80000000
        jz      return0
		test	edx,0x40000000
		jz		return0
        jmp     return1




;void synth_internal_3dnow(sample * samples,unsigned real * b0,real * window,int bo1);

extern _synth_sampleconv_scale,_synth_sampleconv_scale_neg

align 32

global _synth_internal_3dnow
_synth_internal_3dnow:
	push ebx
	push esi
	push edi
	mov edi,[esp+12+4+0];samples
	mov ebx,[esp+12+4+4];b0
	mov esi,[esp+12+4+8];window
	mov ecx,16
_synth_internal_3dnow_l1:	
	movq mm0,[esi]
	movq mm1,[esi+8]
	movq mm2,[esi+16]
	movq mm3,[esi+24]
	movq mm4,[esi+32]
	movq mm5,[esi+40]
	movq mm6,[esi+48]
	movq mm7,[esi+56]
	pfmul mm0,[ebx]
	pfmul mm1,[ebx+8]
	pfmul mm2,[ebx+16]
	pfmul mm3,[ebx+24]
	pfmul mm4,[ebx+32]
	pfmul mm5,[ebx+40]
	pfmul mm6,[ebx+48]
	pfmul mm7,[ebx+56]
	pfadd mm0,mm1
	pfadd mm2,mm3
	pfadd mm4,mm5
	pfadd mm6,mm7
	pfadd mm0,mm2
	pfadd mm4,mm6
	lea esi,[esi+0x20*4]
	pfadd mm0,mm4
	lea ebx,[ebx+0x10*4]
	pfpnacc mm0,mm0
	pfmul mm0,qword [_synth_sampleconv_scale]
	movd dword [edi],mm0
	dec ecx
	lea edi,[edi+2*4]
	jnz _synth_internal_3dnow_l1

	movq mm0,[esi]
	movq mm1,[esi+8]
	movq mm2,[esi+16]
	movq mm3,[esi+24]
	movq mm4,[esi+32]
	movq mm5,[esi+40]
	movq mm6,[esi+48]
	movq mm7,[esi+56]
	pfmul mm0,[ebx]
	pfmul mm1,[ebx+8]
	pfmul mm2,[ebx+16]
	pfmul mm3,[ebx+24]
	pfmul mm4,[ebx+32]
	pfmul mm5,[ebx+40]
	pfmul mm6,[ebx+48]
	pfmul mm7,[ebx+56]
	pfadd mm0,mm1
	pfadd mm2,mm3
	pfadd mm4,mm5
	pfadd mm6,mm7
	pfadd mm0,mm2
	pfadd mm4,mm6
	mov ecx,[esp+12+4+12]
	pfadd mm0,mm4
	sub ebx,0x10*4
	pfmul mm0,qword [_synth_sampleconv_scale]
	sub esi,0x20*4
	movd dword [edi],mm0	
	shl ecx,3
	lea edi,[edi+2*4]
	add esi,ecx

	mov ecx,15
_synth_internal_3dnow_l2:	
	pswapd mm0,[esi-8]
	pswapd mm1,[esi-16]
	movd   mm7,[esi-60]
	pswapd mm2,[esi-24]
	pswapd mm3,[esi-32]
	pswapd mm4,[esi-40]
	pswapd mm5,[esi-48]
	pswapd mm6,[esi-56]
	punpckldq mm7,[esi]
	pfmul mm0,[ebx]
	pfmul mm1,[ebx+8]
	pfmul mm2,[ebx+16]
	pfmul mm3,[ebx+24]
	pfmul mm4,[ebx+32]
	pfmul mm5,[ebx+40]
	pfmul mm6,[ebx+48]
	pfmul mm7,[ebx+56]
	pfadd mm0,mm1
	pfadd mm2,mm3
	pfadd mm4,mm5
	pfadd mm6,mm7
	pfadd mm0,mm2
	pfadd mm4,mm6
	lea esi,[esi-0x20*4]
	pfadd mm0,mm4
	pfacc mm0,mm0
	lea ebx,[ebx-0x10*4]
	pfmul mm0,qword [_synth_sampleconv_scale_neg]
	movd dword [edi],mm0	
	dec ecx
	lea edi,[edi+2*4]
	jnz _synth_internal_3dnow_l2
	pop edi
	pop esi
	pop ebx
	femms
	ret

extern _ms_stereo_extrascalefactor;

;void __fastcall do_ms_stereo_x87(real * in0,real * in1,unsigned count);
%if 0
			  static const real extrascalefactor = 1.0 / sqrt(2.0);
				int i;
				for(i=0;i<SBLIMIT*SSLIMIT;i++) {
					real tmp0,tmp1;
					tmp0 = ((real *) hybridIn[0])[i] * extrascalefactor;
					tmp1 = ((real *) hybridIn[1])[i] * extrascalefactor;
					((real *) hybridIn[1])[i] = tmp0 - tmp1;  
					((real *) hybridIn[0])[i] = tmp0 + tmp1;
				}
%endif

align 32
global _do_ms_stereo_3dnow
_do_ms_stereo_3dnow:		;ecx: in0, edx:in1
	mov eax,dword [esp+4]
	movq mm7,qword [_ms_stereo_extrascalefactor]
	shr eax,2
_do_ms_stereo_3dnow_l1:
	movq mm0,qword [ecx]
	movq mm1,qword [edx]	;t0, t1
	pfmul mm0,mm7
	pfmul mm1,mm7
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	movq qword [edx],mm0
	movq qword [ecx],mm2		

	movq mm0,qword [ecx+8]
	movq mm1,qword [edx+8]	;t0, t1
	pfmul mm0,mm7
	pfmul mm1,mm7
	movq mm2,mm0
	pfsub mm0,mm1
	pfadd mm2,mm1
	movq qword [edx+8],mm0
	movq qword [ecx+8],mm2
		
	dec eax	
	lea ecx,[ecx+16]
	lea edx,[edx+16]
	jnz _do_ms_stereo_3dnow_l1
	femms
	ret 4

align 32
global _do_ms_stereo_x87
_do_ms_stereo_x87:		;ecx: in0, edx:in1
	mov eax,dword [esp+4]
	fld dword [_ms_stereo_extrascalefactor]
	shr eax,1
_do_ms_stereo_x87_l1:
	fld dword [ecx]
	fmul st0,st1
	fld dword [edx]
	fmul st0,st2; t1,t0,ex
	fld st1	;t0 t1 t0 ex
	fsub st0,st1
	fstp dword [edx]
	faddp st1,st0
	fstp dword [ecx]

	fld dword [ecx+4]
	fmul st0,st1
	fld dword [edx+4]
	fmul st0,st2; t1,t0,ex
	fld st1	;t0 t1 t0 ex
	fsub st0,st1
	fstp dword [edx+4]
	faddp st1,st0
	fstp dword [ecx+4]

	dec eax	
	lea ecx,[ecx+8]
	lea edx,[edx+8]
	jnz _do_ms_stereo_x87_l1
	fstp st0
	ret 4