; ========================================================
; Benchmark: Recursive Factorial Long (Scaled >= 100k instructions)
; Computes 10! = 3628800 (0x375F00) repeatedly in a loop.
; Total retired instructions: ~114,400
; ========================================================
.text
    mov r8, 800        ; Outer loop count = 800 iterations

.outer_loop:
    mov r0, 10         ; Input n = 10
    call .factorial    ; invoke factorial(n)

    sub r8, r8, 1
    cmp r8, 0
    bgt .outer_loop
    b .exit

.factorial:
    cmp r0, 1          ; check base case n <= 1
    beq .base
    bgt .recurse
    mov r1, 1
    ret

.base:
    mov r1, 1          ; base case return 1
    ret

.recurse:
    push r0            ; push n onto full-descending stack
    push ra            ; push return address onto stack
    sub r0, r0, 1      ; n = n - 1
    call .factorial    ; recursive call, result in r1
    pop ra             ; restore return address
    pop r0             ; restore n
    mul r1, r0, r1     ; r1 = n * factorial(n-1)
    ret

.exit:
    nop

