; ========================================================
; Benchmark: Hazard Stress Test Long (Scaled >= 100k instructions)
; Tests RAW forwarding, load-use stall, load-store forwarding,
; and branch resolution in an outer loop.
; Total retired instructions: ~110,000
; ========================================================
.text
    movh r8, 0x0000
    addu r8, r8, 0x2000

    mov r14, 6000      ; Outer loop count = 6000 iterations

.outer_loop:
    ; Base registers
    mov r2, 10
    mov r3, 20
    mov r5, 5

    ; Test 1: Intense back-to-back ALU RAW forwarding
    add r1, r2, r3     ; r1 = 10 + 20 = 30
    sub r4, r1, r5     ; r4 = 30 - 5 = 25
    mul r6, r4, r1     ; r6 = 25 * 30 = 750

    ; Test 2: Store then Load-Use Hazard
    st r6, 0[r8]       ; store 750 into mem[0x2000]
    ld r9, 0[r8]       ; load from mem[0x2000] into r9
    add r10, r9, 50    ; LOAD-USE HAZARD: r10 = 750 + 50 = 800

    ; Test 3: Load followed by Store (zero-stall forwarding)
    ld r11, 0[r8]      ; load 750
    st r11, 4[r8]      ; store forwarded

    ; Test 4: Branch flush test
    cmp r10, 800
    beq .target
    add r12, r12, 999  ; Wrong path
    add r13, r13, 999  ; Wrong path

.target:
    mov r12, 42        ; Correct path

    sub r14, r14, 1
    cmp r14, 0
    bgt .outer_loop

.exit:
    nop

