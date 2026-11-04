; ========================================================
; Benchmark: Stack Macro Test Long (Scaled >= 100k instructions)
; Tests pushm, popm, and stack operations repeatedly in a loop.
; Total retired instructions: ~114,000
; ========================================================
.text
    mov r8, 6000       ; Outer loop count = 6000 iterations

.outer_loop:
    mov r1, 111
    mov r2, 222
    mov r3, 333
    mov r4, 444

    pushm {r1, r2, r3, r4}

    mov r1, 0
    mov r2, 0
    mov r3, 0
    mov r4, 0

    popm {r1, r2, r3, r4}

    sub r8, r8, 1
    cmp r8, 0
    bgt .outer_loop

.exit:
    nop

