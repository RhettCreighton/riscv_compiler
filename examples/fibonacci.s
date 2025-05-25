# Fibonacci sequence calculator
# Calculates the 10th Fibonacci number

.section .text
.global _start

_start:
    li x1, 0        # fib(0) = 0
    li x2, 1        # fib(1) = 1
    li x3, 10       # calculate fib(10)
    li x4, 2        # counter = 2
    
fib_loop:
    add x5, x1, x2  # next = fib(n-2) + fib(n-1)
    mv x1, x2       # fib(n-2) = fib(n-1)
    mv x2, x5       # fib(n-1) = next
    addi x4, x4, 1  # counter++
    ble x4, x3, fib_loop  # if counter <= 10, continue
    
    # Result is in x2 (should be 55)
    li x6, 0x1000   # result address
    sw x2, 0(x6)    # store result
    
end:
    j end           # infinite loop