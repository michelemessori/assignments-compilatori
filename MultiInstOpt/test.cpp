#include <stdio.h>

int test_add_then_sub(int b) {
    int a = b + 5;
    int c = a - 5; 
    return c; // Il nostro pass lo trasformerà in 'return b'
}

int test_sub_then_add(int b) {
    int a = b - 10;
    int c = a + 10;
    return c; // Il nostro pass lo trasformerà in 'return b'
}

int test_fake(int b) {
    int a = b + 5;
    int c = a - 4; // Costanti diverse, il pass lo ignorerà
    return c;
}

int test_add_then_sub_used_twice(int b) {
    int a = b + 5;
    int c = a - 5;
    return c + a; // c diventa b, ma 'a' esiste ancora per essere sommato!
}