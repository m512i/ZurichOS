; Interrupt Service Routines (ISR) and IRQ Stubs
; Assembly handlers that call C interrupt handlers

bits 32

extern isr_handler
extern irq_handler
KERNEL_DATA_SEG equ 0x10

isr_common_stub:
    pusha
    mov ax, ds
    push eax
    
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    
    call isr_handler
    
    add esp, 4
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa
    
    add esp, 8
    
    iret

irq_common_stub:
    pusha
    
    mov ax, ds
    push eax
    
    mov ax, KERNEL_DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp
    
    call irq_handler
    
    add esp, 4
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa
    
    add esp, 8
    
    iret

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0        
    push dword %1       
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1       
    jmp isr_common_stub
%endmacro

; CPU Exceptions 0-31
ISR_NOERRCODE 0     ; Division By Zero
ISR_NOERRCODE 1     ; Debug
ISR_NOERRCODE 2     ; Non Maskable Interrupt
ISR_NOERRCODE 3     ; Breakpoint
ISR_NOERRCODE 4     ; Into Detected Overflow
ISR_NOERRCODE 5     ; Out of Bounds
ISR_NOERRCODE 6     ; Invalid Opcode
ISR_NOERRCODE 7     ; No Coprocessor
ISR_ERRCODE   8     ; Double Fault
ISR_NOERRCODE 9     ; Coprocessor Segment Overrun
ISR_ERRCODE   10    ; Bad TSS
ISR_ERRCODE   11    ; Segment Not Present
ISR_ERRCODE   12    ; Stack Fault
ISR_ERRCODE   13    ; General Protection Fault
ISR_ERRCODE   14    ; Page Fault
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; Coprocessor Fault
ISR_ERRCODE   17    ; Alignment Check
ISR_NOERRCODE 18    ; Machine Check
ISR_NOERRCODE 19    ; SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; Virtualization Exception
ISR_ERRCODE   21    ; Control Protection Exception
ISR_NOERRCODE 22    ; Reserved
ISR_NOERRCODE 23    ; Reserved
ISR_NOERRCODE 24    ; Reserved
ISR_NOERRCODE 25    ; Reserved
ISR_NOERRCODE 26    ; Reserved
ISR_NOERRCODE 27    ; Reserved
ISR_NOERRCODE 28    ; Reserved
ISR_NOERRCODE 29    ; Reserved
ISR_ERRCODE   30    ; Security Exception
ISR_NOERRCODE 31    ; Reserved

; IRQ stubs (HW Interrupts 32-47)

%macro IRQ 2
global irq%1
irq%1:
    push dword 0       
    push dword %2       
    jmp irq_common_stub
%endmacro

IRQ 0, 32   ; Timer
IRQ 1, 33   ; Keyboard
IRQ 2, 34   ; Cascade
IRQ 3, 35   ; COM2
IRQ 4, 36   ; COM1
IRQ 5, 37   ; LPT2
IRQ 6, 38   ; Floppy
IRQ 7, 39   ; LPT1 / Spurious
IRQ 8, 40   ; CMOS RTC
IRQ 9, 41   ; Free
IRQ 10, 42  ; Free
IRQ 11, 43  ; Free
IRQ 12, 44  ; PS/2 Mouse
IRQ 13, 45  ; FPU
IRQ 14, 46  ; Primary ATA
IRQ 15, 47  ; Secondary ATA

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    
    mov ax, 0x10        
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    jmp 0x08:.flush
.flush:
    ret

global tss_flush
tss_flush:
    mov ax, 0x48        ; TSS segment selector (index 9)
    ltr ax
    ret

global idt_flush
idt_flush:
    mov eax, [esp + 4]  
    lidt [eax]         
    ret

global isr128
isr128:
    push dword 0        
    push dword 128      ; INT 0x80
    jmp isr_common_stub

; INT 0x81 - Driver kernel service call (Ring 1 -> Ring 0)
; Called from Ring 1 via: INT 0x81
; EAX = service_id, EBX = arg1, ECX = arg2, EDX = arg3
; Return value in EAX
global isr129
isr129:
    push dword 0
    push dword 129      ; INT 0x81
    jmp isr_common_stub

; INT 0x82 - Driver return (Ring 1 -> Ring 0)
; Called from Ring 1 trampoline when driver function completes
; EAX = return value from driver function
global isr130
isr130:
    push dword 0
    push dword 130      ; INT 0x82
    jmp isr_common_stub

; ring1_enter - Transition from Ring 0 to Ring 1 via IRET
; C prototype: void ring1_enter(uint32_t ring1_cs, uint32_t ring1_ds,
;                                uint32_t ring1_esp, uint32_t ring1_eip);
; Stack on entry: [ret_addr] [ring1_cs] [ring1_ds] [ring1_esp] [ring1_eip]
global ring1_enter
ring1_enter:
    mov eax, [esp + 4]    
    mov ebx, [esp + 8]     
    mov ecx, [esp + 12]   
    mov edx, [esp + 16]    

    ; Build IRET frame on Ring 0 stack for transition to Ring 1.
    ; DS/ES/FS/GS stay as Ring 0 segments during frame construction.
    ; The Ring 1 trampoline will load Ring 1 data segments after landing.
    push ebx               ; SS = Ring 1 data segment
    push ecx               ; ESP = Ring 1 stack pointer
    pushfd
    pop ecx                ; get current EFLAGS
    and ecx, ~(3 << 12)    ; clear IOPL bits (IOPL=0, IOPB enforced)
    or  ecx, (1 << 9)      ; ensure IF=1 (interrupts enabled)
    push ecx               ; EFLAGS
    push eax               ; CS = Ring 1 code segment
    push edx               ; EIP = Ring 1 entry point

    ; Load Ring 1 data segments right before IRET.
    ; The pushes above are done, so DS is no longer needed for stack ops.
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    iret                   
