void simple_fusion(int *a, int *b, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = i;

  for (int i = 0; i < n; ++i)
    b[i] = a[i];
}
