; ========================================================
; Benchmark: Iterative Factorial Long (Scaled >= 100k instructions)
; Computes 6! = 720 (0x2D0) repeatedly in a loop.
; Total retired instructions: ~130,000
; ========================================================
.text
    mov r8, 5000       ; Outer loop count = 5000 iterations

.outer_loop:
    mov r0, 6          ; Input number n = 6
    mov r1, 1          ; prod = 1
    mov r2, r0         ; idx = n

.inner_loop:
    mul r1, r1, r2     ; prod = prod * idx
    sub r2, r2, 1      ; idx = idx - 1
    cmp r2, 1          ; compare (idx, 1)
    bgt .inner_loop    ; if idx > 1 continue loop

    sub r8, r8, 1
    cmp r8, 0
    bgt .outer_loop

.exit:
    nop

