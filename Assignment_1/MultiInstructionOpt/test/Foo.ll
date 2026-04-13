; int foo(int e, int a) {
;   int b = a + 1;
;   int c = b -1;
;   b = e * 3;
;   int d = b / 3;
;   return c * d;
; }

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %3 = add nsw i32 %1, 1
  %4 = sub nsw i32 %3, 1
  %5 = mul nsw i32 %0, 3
  %6 = sdiv i32 %5, 3
  %7 = mul nsw i32 %4, %6
  ret i32 %7
}
