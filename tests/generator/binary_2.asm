format PE console
entry _main
include '%fasm%/include/win32a.inc'

section '.text' code executable
; start main
_main:
	push 14
	push 5
	pop ebx
	pop eax
	or eax, ebx
	push eax
	push _@0
	call [_printf]
	add esp, 8
	push 9
	push 7
	pop ebx
	pop eax
	xor eax, ebx
	push eax
	push _@1
	call [_printf]
	add esp, 8
	push 74
	push 13
	pop ebx
	pop eax
	and eax, ebx
	push eax
	push _@2
	call [_printf]
	add esp, 8
	push 1
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jz _@3
	test ebx, ebx
_@3:
	setnz cl
	mov eax, ecx
	push eax
	push _@4
	call [_printf]
	add esp, 8
	push 0
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jz _@5
	test ebx, ebx
_@5:
	setnz cl
	mov eax, ecx
	push eax
	push _@6
	call [_printf]
	add esp, 8
	push 10
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jz _@7
	test ebx, ebx
_@7:
	setnz cl
	mov eax, ecx
	push eax
	push _@8
	call [_printf]
	add esp, 8
	push 0
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jnz _@9
	test ebx, ebx
_@9:
	setnz cl
	mov eax, ecx
	push eax
	push _@10
	call [_printf]
	add esp, 8
	push 0
	push 7
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jnz _@11
	test ebx, ebx
_@11:
	setnz cl
	mov eax, ecx
	push eax
	push _@12
	call [_printf]
	add esp, 8
	push 5
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	test eax, eax
	jnz _@13
	test ebx, ebx
_@13:
	setnz cl
	mov eax, ecx
	push eax
	push _@14
	call [_printf]
	add esp, 8
	push 0
	call [_ExitProcess]
; end main

section '.data' data readable writable

_@0 db 37,100,10,0
_@1 db 37,100,10,0
_@2 db 37,100,10,0
_@4 db 37,100,10,0
_@6 db 37,100,10,0
_@8 db 37,100,10,0
_@10 db 37,100,10,0
_@12 db 37,100,10,0
_@14 db 37,100,10,0

section '.idata' data readable import
library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'
import kernel32, _ExitProcess, 'ExitProcess'
import msvcrt, _printf, 'printf'
