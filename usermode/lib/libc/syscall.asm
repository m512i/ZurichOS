; User-space system call wrappers

bits 32

section .text

global syscall0
syscall0:
    mov eax, [esp + 4]  ; syscall number
    int 0x80
    ret

global syscall1
syscall1:
    push ebx
    mov eax, [esp + 8]  ; syscall number
    mov ebx, [esp + 12] ; arg1
    int 0x80
    pop ebx
    ret

global syscall2
syscall2:
    push ebx
    mov eax, [esp + 8]  ; syscall number
    mov ebx, [esp + 12] ; arg1
    mov ecx, [esp + 16] ; arg2
    int 0x80
    pop ebx
    ret

global syscall3
syscall3:
    push ebx
    mov eax, [esp + 8]  ; syscall number
    mov ebx, [esp + 12] ; arg1
    mov ecx, [esp + 16] ; arg2
    mov edx, [esp + 20] ; arg3
    int 0x80
    pop ebx
    ret

global _exit
_exit:
    mov eax, 0          ; SYS_EXIT
    mov ebx, [esp + 4]  ; status
    int 0x80
    jmp $               ; Should never return

global write
write:
    push ebx
    mov eax, 2          ; SYS_WRITE
    mov ebx, [esp + 8]  ; fd
    mov ecx, [esp + 12] ; buf
    mov edx, [esp + 16] ; count
    int 0x80
    pop ebx
    ret

global read
read:
    push ebx
    mov eax, 1          ; SYS_READ
    mov ebx, [esp + 8]  ; fd
    mov ecx, [esp + 12] ; buf
    mov edx, [esp + 16] ; count
    int 0x80
    pop ebx
    ret
