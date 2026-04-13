; ModuleID = 'Foo.ll'
source_filename = "Foo.ll"

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) {
  %3 = add nsw i32 %1, 2
  %4 = mul nsw i32 %3, 5
  %5 = shl i32 %3, 2
  %6 = add i32 %5, %3
  %7 = shl i32 %0, 1
  %8 = sdiv i32 %7, 4
  %9 = lshr i32 %7, 2
  %10 = mul nsw i32 %4, %9
  ret i32 %10
}
