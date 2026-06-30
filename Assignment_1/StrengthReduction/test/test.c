int foo(unsigned int x) {
    int a = x * 8;
    int b = x * 7;
    int c = x * 9;
    unsigned int d = x / 4;


    int y = a+b;
    int z = c-d;
    return y*z;
}
