format PE console
entry _main
include '%fasm%/include/win32a.inc'

section '.text' code executable
; start main
_main:
	push 1
	push 2
	pop ebx
	pop eax
	add eax, ebx
	push eax
	push _@0
	call [_printf]
	add esp, 8
	push 1
	push 2
	pop ebx
	pop eax
	sub eax, ebx
	push eax
	push _@1
	call [_printf]
	add esp, 8
	push 1
	push 2
	pop ebx
	pop eax
	imul ebx
	push eax
	push _@2
	call [_printf]
	add esp, 8
	push 7
	push 2
	pop ebx
	pop eax
	cdq
	idiv ebx
	push eax
	push _@3
	call [_printf]
	add esp, 8
	push 7
	push 2
	pop ebx
	pop eax
	cdq
	idiv ebx
	mov eax, edx
	push eax
	push _@4
	call [_printf]
	add esp, 8
	push 1
	push 3
	pop ebx
	pop eax
	mov ecx, ebx
	sal eax, cl
	push eax
	push _@5
	call [_printf]
	add esp, 8
	push 7
	push 1
	pop ebx
	pop eax
	mov ecx, ebx
	sar eax, cl
	push eax
	push _@6
	call [_printf]
	add esp, 8
	push 0
	call [_ExitProcess]
; end main

section '.data' data readable writable

_@0 db 37,100,10,0
_@1 db 37,100,10,0
_@2 db 37,100,10,0
_@3 db 37,100,10,0
_@4 db 37,100,10,0
_@5 db 37,100,10,0
_@6 db 37,100,10,0

section '.idata' data readable import
library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'
import kernel32, _ExitProcess, 'ExitProcess'
import msvcrt, _printf, 'printf'
