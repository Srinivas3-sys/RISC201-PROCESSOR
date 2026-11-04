; ========================================================
; Benchmark: Array Summation Long (Scaled >= 100k instructions)
; Initializes array of 20 elements, then sums repeatedly in a loop.
; Total retired instructions: ~160,000
; ========================================================
.text
    ; Base address of array at 0x1000
    movh r1, 0x0000
    addu r1, r1, 0x1000

    ; Store 20 elements into memory
    mov r3, 10
    st r3, 0[r1]
    mov r3, 20
    st r3, 4[r1]
    mov r3, 30
    st r3, 8[r1]
    mov r3, 40
    st r3, 12[r1]
    mov r3, 50
    st r3, 16[r1]
    mov r3, 60
    st r3, 20[r1]
    mov r3, 70
    st r3, 24[r1]
    mov r3, 80
    st r3, 28[r1]
    mov r3, 90
    st r3, 32[r1]
    mov r3, 100
    st r3, 36[r1]
    mov r3, 110
    st r3, 40[r1]
    mov r3, 120
    st r3, 44[r1]
    mov r3, 130
    st r3, 48[r1]
    mov r3, 140
    st r3, 52[r1]
    mov r3, 150
    st r3, 56[r1]
    mov r3, 160
    st r3, 60[r1]
    mov r3, 170
    st r3, 64[r1]
    mov r3, 180
    st r3, 68[r1]
    mov r3, 190
    st r3, 72[r1]
    mov r3, 200
    st r3, 76[r1]

    mov r8, 1000       ; Outer loop count = 1000 iterations

.outer_loop:
    mov r0, 0          ; sum = 0
    mov r2, 0          ; idx = 0
    mov r4, 20         ; limit = 20

.inner_loop:
    cmp r2, r4
    beq .sum_done
    lsl r5, r2, 2      ; byte offset = idx * 4
    add r6, r1, r5     ; effective address = base + offset
    ld r7, 0[r6]       ; load array[idx]
    add r0, r0, r7     ; sum += array[idx]
    add r2, r2, 1      ; idx++
    b .inner_loop

.sum_done:
    sub r8, r8, 1
    cmp r8, 0
    bgt .outer_loop

.exit:
    nop

