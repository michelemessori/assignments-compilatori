#include <stdio.h>

// --- CASI VALIDI (Verranno ottimizzati a un semplice 'return x') ---

int test_add_zero_right(int x) { return x + 0; }
int test_add_zero_left(int x)  { return 0 + x; }

int test_mul_one_right(int x)  { return x * 1; }
int test_mul_one_left(int x)   { return 1 * x; }

int test_sub_zero_right(int x) { return x - 0; }
int test_div_one_right(int x)  { return x / 1; }

// --- CASI TRAPPOLA (NON verranno toccati dal Pass) ---

int test_sub_zero_left(int x)  { return 0 - x; } // Restituisce -x
int test_div_one_left(int x)   { return 1 / x; } // Divisione intera 1/x

int main() {
    // Corpo vuoto sufficiente per generare l'IR
    return 0;
}