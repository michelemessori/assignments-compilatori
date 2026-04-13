; ModuleID = 'Foo.ll'
source_filename = "Foo.ll"

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) {
  %3 = add nsw i32 %1, 1
  %4 = sub nsw i32 %3, 1
  %5 = add i32 %1, 0
  %6 = mul nsw i32 %0, 3
  %7 = sdiv i32 %6, 3
  %8 = add i32 %0, 0
  %9 = mul nsw i32 %5, %8
  ret i32 %9
}
