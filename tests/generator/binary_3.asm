format PE console
entry _main
include '%fasm%/include/win32a.inc'

section '.text' code executable
; start main
_main:
	push 1
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setne cl
	mov eax, ecx
	push eax
	push _@0
	call [_printf]
	add esp, 8
	push 1
	push 2
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setne cl
	mov eax, ecx
	push eax
	push _@1
	call [_printf]
	add esp, 8
	push 1
	push 4
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	sete cl
	mov eax, ecx
	push eax
	push _@2
	call [_printf]
	add esp, 8
	push 1
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	sete cl
	mov eax, ecx
	push eax
	push _@3
	call [_printf]
	add esp, 8
	push 1
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setl cl
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
	cmp eax, ebx
	setl cl
	mov eax, ecx
	push eax
	push _@5
	call [_printf]
	add esp, 8
	push 1
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setle cl
	mov eax, ecx
	push eax
	push _@6
	call [_printf]
	add esp, 8
	push 0
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setle cl
	mov eax, ecx
	push eax
	push _@7
	call [_printf]
	add esp, 8
	push 1
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setg cl
	mov eax, ecx
	push eax
	push _@8
	call [_printf]
	add esp, 8
	push 1
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setg cl
	mov eax, ecx
	push eax
	push _@9
	call [_printf]
	add esp, 8
	push 0
	push 1
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setge cl
	mov eax, ecx
	push eax
	push _@10
	call [_printf]
	add esp, 8
	push 0
	push 0
	pop ebx
	pop eax
	xor ecx, ecx
	cmp eax, ebx
	setge cl
	mov eax, ecx
	push eax
	push _@11
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
_@7 db 37,100,10,0
_@8 db 37,100,10,0
_@9 db 37,100,10,0
_@10 db 37,100,10,0
_@11 db 37,100,10,0

section '.idata' data readable import
library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'
import kernel32, _ExitProcess, 'ExitProcess'
import msvcrt, _printf, 'printf'
