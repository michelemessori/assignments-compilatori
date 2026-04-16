#include <stdio.h>

// 1. Moltiplicazione per potenza di 2
int test_mul_pow2(int x) {
    return x * 4; // LLVM IR: x * 4 --> Pass: x << 2
}

// 2. Moltiplicazione per successore di potenza di 2
int test_mul_succ(int x) {
    return x * 5; // LLVM IR: x * 5 --> Pass: (x << 2) + x
}

// 3. Moltiplicazione per predecessore di potenza di 2
int test_mul_pred(int x) {
    return x * 3; // LLVM IR: x * 3 --> Pass: (x << 2) - x
}

// 4. Divisione senza segno (UDiv) per potenza di 2
unsigned int test_udiv_pow2(unsigned int x) {
    return x / 8; // LLVM IR: x / 8 --> Pass: x >> 3 (Logical)
}

// 5. Divisione con segno (SDiv) per potenza di 2
int test_sdiv_pow2(int x) {
    return x / 8; // LLVM IR: x / 8 --> Pass: x >> 3 (Arithmetic)
}

int main() {
    int val = 10;
    printf("Mul x*4: %d\n", test_mul_pow2(val));
    printf("Mul x*5: %d\n", test_mul_succ(val));
    printf("Mul x*3: %d\n", test_mul_pred(val));
    printf("UDiv x/8: %u\n", test_udiv_pow2((unsigned int)val));
    printf("SDiv x/8: %d\n", test_sdiv_pow2(val));
    return 0;
}