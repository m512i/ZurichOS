bits 32

section .text

global _start
extern main
extern _init
extern _fini

_start:
    xor ebp, ebp
    
    ; Get argc, argv from stack pushed by kernel
    ; Stack layout: [argc] [argv[0]] [argv[1]] ... [NULL] [envp...]
    pop esi             ; argc
    mov ecx, esp        ; argv
    
    ; Align stack to 16 bytes required by some ABIs
    and esp, 0xFFFFFFF0
    push ecx            
    push esi            
    call _init
    call main
    mov ebx, eax
    call _fini
    
    mov eax, 0          ; SYS_EXIT
    int 0x80
    
    ;  Should never reach here
    jmp $

; Placeholder init/fini if not defined
global _init
global _fini

section .init
_init:
    ret

section .fini
_fini:
    ret
