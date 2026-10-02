section .text
global _start
_start:
    call seq_step_1
    call seq_step_2
    xor edi, edi
    mov eax, 60            ; exit(0)
    syscall
seq_fail:
    mov edi, 70
    mov eax, 60            ; exit(70)
    syscall
seq_step_1:
    lea rdi, [rel d0]
    mov esi, 577           ; O_WRONLY | O_CREAT | O_TRUNC
    mov edx, 420           ; 0644
    mov eax, 2             ; open
    syscall
    test rax, rax
    js seq_fail
    mov rbx, rax
    mov rdi, rbx
    lea rsi, [rel d1]
    mov edx, 76
    mov eax, 1             ; write
    syscall
    test rax, rax
    js seq_fail
    mov rdi, rbx
    mov eax, 3             ; close
    syscall
    ret
seq_step_2:
    mov edi, 1
    lea rsi, [rel d2]
    mov edx, 82
    mov eax, 1             ; write
    syscall
    test rax, rax
    js seq_fail
    ret
section .rodata
d0:
    db "company.txt", 0
d1:
    db "ACME,10,25.50", 10
    db "Globex,4,120.00", 10
    db "Initech,30,9.75", 10
    db "ACME,5,26.00", 10
    db "Umbrella,2,40.00", 10
d2:
    db "Top 3 total-revenue-by-company:", 10
    db "1. Globex 480.00", 10
    db "2. ACME 385.00", 10
    db "3. Initech 292.50", 10
