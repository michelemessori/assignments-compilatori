; ModuleID = 'test.m2r.rotated.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @simple_fusion(ptr noundef %0, ptr noundef %1, i32 noundef %2) #0 {
  %4 = icmp slt i32 0, %2
  br i1 %4, label %.lr.ph, label %19

.lr.ph:                                           ; preds = %3
  br label %5

5:                                                ; preds = %17, %.lr.ph
  %.011 = phi i32 [ 0, %.lr.ph ], [ %9, %17 ]
  %6 = sext i32 %.011 to i64
  %7 = getelementptr inbounds i32, ptr %0, i64 %6
  store i32 %.011, ptr %7, align 4
  br label %8

8:                                                ; preds = %5
  %9 = add nsw i32 %.011, 1
  %10 = icmp slt i32 %9, %2
  br label %11

11:                                               ; preds = %8
  %12 = sext i32 %.011 to i64
  %13 = getelementptr inbounds i32, ptr %0, i64 %12
  %14 = load i32, ptr %13, align 4
  %15 = sext i32 %.011 to i64
  %16 = getelementptr inbounds i32, ptr %1, i64 %15
  store i32 %14, ptr %16, align 4
  br label %17

17:                                               ; preds = %11
  %18 = icmp slt i32 %9, %2
  br i1 %18, label %5, label %._crit_edge5, !llvm.loop !6

._crit_edge5:                                     ; preds = %17
  br label %19

19:                                               ; preds = %3, %._crit_edge5
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 19.1.7 (/home/runner/work/llvm-project/llvm-project/clang cd708029e0b2869e80abe31ddb175f7c35361f90)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
