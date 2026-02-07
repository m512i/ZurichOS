; Context Switch Assembly
; void context_switch(uint32_t *old_esp, uint32_t new_esp)
; Saves current context and switches to new task

[BITS 32]

global context_switch

section .text

; context_switch(uint32_t *old_esp, uint32_t new_esp)
; Arguments:
;   [esp+4] = pointer to old task's ESP storage
;   [esp+8] = new task's ESP value
context_switch:
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 20]     ; old_esp pointer (4 regs + ret addr = 20 bytes offset)
    mov [eax], esp
    mov esp, [esp + 24]     
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret