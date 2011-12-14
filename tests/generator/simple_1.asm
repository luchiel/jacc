format PE console
entry _main
include '%fasm%/include/win32a.inc'

section '.text' code executable
; start main
_main:
	push _@0
	call [_printf]
	add esp, 4
	push 0
	call [_ExitProcess]
; end main

section '.data' data readable writable

_@0 db 73,116,32,119,111,114,107,115,33,0

section '.idata' data readable import
library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'
import kernel32, _ExitProcess, 'ExitProcess'
import msvcrt, _printf, 'printf'
