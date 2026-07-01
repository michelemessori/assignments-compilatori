int fun(int n, int a, int b, int c) {
    int sum = 0;

    for (int i = 0; i < n; i++) {
        int x = a + b;      // loop invariant
        int y = c * 2;      // loop invariant

        sum += x * y + i;
    }

    return sum;
}
