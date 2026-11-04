; ========================================================
; Benchmark: Prime Test Long (Scaled >= 100k instructions)
; Verifies prime property of 29 repeatedly in a loop.
; Total retired instructions: ~130,000
; ========================================================
.text
    mov r8, 800        ; Outer loop count = 800 iterations

.outer_loop:
    mov r1, 29         ; test number = 29
    mov r2, 2          ; divisor = 2

.inner_loop:
    mod r3, r1, r2     ; r3 = r1 % r2
    cmp r3, 0          ; check remainder == 0
    beq .notprime      ; if remainder == 0, composite!
    add r2, r2, 1      ; divisor++
    cmp r1, r2         ; compare r1 with divisor
    bgt .inner_loop    ; while divisor < r1, continue

    mov r0, 1          ; number is prime
    b .next_iter

.notprime:
    mov r0, 0          ; number is composite

.next_iter:
    sub r8, r8, 1
    cmp r8, 0
    bgt .outer_loop

.exit:
    nop

