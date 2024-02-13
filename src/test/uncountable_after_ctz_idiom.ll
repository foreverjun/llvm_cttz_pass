; ModuleID = 'uncountable_before_loop_idiom.ll'
source_filename = "ctz_uncountable_wc.c"
target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128"
target triple = "riscv64-gcc-linux-gnu"

; Function Attrs: nofree norecurse nosync nounwind memory(none) uwtable
define signext i32 @count_trailing_zeroes(i32 noundef signext %0) local_unnamed_addr #0 {
  %2 = icmp ne i32 %0, 0
  %3 = and i32 %0, 1
  %4 = icmp eq i32 %3, 0
  %or.cond = and i1 %2, %4
  br i1 %or.cond, label %.lr.ph.preheader, label %.loopexit

.lr.ph.preheader:                                 ; preds = %1
  %5 = call i32 @llvm.cttz.i32(i32 %0, i1 true)
  br label %.lr.ph

.lr.ph:                                           ; preds = %.lr.ph.preheader, %.lr.ph
  %tcphi = phi i32 [ %5, %.lr.ph.preheader ], [ %tcdec, %.lr.ph ]
  %.010 = phi i32 [ %6, %.lr.ph ], [ 0, %.lr.ph.preheader ]
  %.069 = phi i32 [ %7, %.lr.ph ], [ %0, %.lr.ph.preheader ]
  %6 = add nuw nsw i32 %.010, 1
  %7 = ashr i32 %.069, 1
  %8 = and i32 %.069, 2
  %tcdec = sub nsw i32 %tcphi, 1
  %9 = icmp ne i32 %tcdec, 0
  br i1 %9, label %.lr.ph, label %.loopexit.loopexit, !llvm.loop !6

.loopexit.loopexit:                               ; preds = %.lr.ph
  %.lcssa = phi i32 [ %5, %.lr.ph ]
  br label %.loopexit

.loopexit:                                        ; preds = %.loopexit.loopexit, %1
  %.07 = phi i32 [ 0, %1 ], [ %.lcssa, %.loopexit.loopexit ]
  ret i32 %.07
}

; Function Attrs: nofree nounwind uwtable
define signext i32 @main(i32 noundef signext %0, ptr nocapture noundef readonly %1) local_unnamed_addr #1 {
  %3 = getelementptr inbounds ptr, ptr %1, i64 1
  %4 = load ptr, ptr %3, align 8, !tbaa !8
  %5 = tail call i64 @strtol(ptr nocapture noundef nonnull %4, ptr noundef null, i32 noundef signext 10) #4
  %6 = trunc i64 %5 to i32
  %7 = icmp ne i32 %6, 0
  %8 = and i32 %6, 1
  %9 = icmp eq i32 %8, 0
  %or.cond.i = and i1 %7, %9
  br i1 %or.cond.i, label %.lr.ph.i.preheader, label %count_trailing_zeroes.exit

.lr.ph.i.preheader:                               ; preds = %2
  %10 = call i32 @llvm.cttz.i32(i32 %6, i1 true)
  br label %.lr.ph.i

.lr.ph.i:                                         ; preds = %.lr.ph.i.preheader, %.lr.ph.i
  %tcphi = phi i32 [ %10, %.lr.ph.i.preheader ], [ %tcdec, %.lr.ph.i ]
  %.010.i = phi i32 [ %11, %.lr.ph.i ], [ 0, %.lr.ph.i.preheader ]
  %.069.i = phi i32 [ %12, %.lr.ph.i ], [ %6, %.lr.ph.i.preheader ]
  %11 = add nuw nsw i32 %.010.i, 1
  %12 = ashr i32 %.069.i, 1
  %13 = and i32 %.069.i, 2
  %tcdec = sub nsw i32 %tcphi, 1
  %14 = icmp ne i32 %tcdec, 0
  br i1 %14, label %.lr.ph.i, label %count_trailing_zeroes.exit.loopexit, !llvm.loop !6

count_trailing_zeroes.exit.loopexit:              ; preds = %.lr.ph.i
  %.lcssa = phi i32 [ %10, %.lr.ph.i ]
  br label %count_trailing_zeroes.exit

count_trailing_zeroes.exit:                       ; preds = %count_trailing_zeroes.exit.loopexit, %2
  %.07.i = phi i32 [ 0, %2 ], [ %.lcssa, %count_trailing_zeroes.exit.loopexit ]
  ret i32 %.07.i
}

; Function Attrs: mustprogress nofree nounwind willreturn
declare i64 @strtol(ptr noundef readonly, ptr nocapture noundef, i32 noundef signext) local_unnamed_addr #2

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.cttz.i32(i32, i1 immarg) #3

attributes #0 = { nofree norecurse nosync nounwind memory(none) uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+c,+d,+f,+m,+relax,+zba,+zbb,+zbc,+zbs,+zicsr,+zifencei,-e,-experimental-smaia,-experimental-ssaia,-experimental-zacas,-experimental-zfa,-experimental-zfbfmin,-experimental-zicond,-experimental-zihintntl,-experimental-ztso,-experimental-zvbb,-experimental-zvbc,-experimental-zvfbfmin,-experimental-zvfbfwma,-experimental-zvkg,-experimental-zvkn,-experimental-zvknc,-experimental-zvkned,-experimental-zvkng,-experimental-zvknha,-experimental-zvknhb,-experimental-zvks,-experimental-zvksc,-experimental-zvksed,-experimental-zvksg,-experimental-zvksh,-experimental-zvkt,-h,-save-restore,-svinval,-svnapot,-svpbmt,-v,-xcvbitmanip,-xcvmac,-xsfcie,-xsfvcp,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-zawrs,-zbkb,-zbkc,-zbkx,-zca,-zcb,-zcd,-zce,-zcf,-zcmp,-zcmt,-zdinx,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zicbom,-zicbop,-zicboz,-zicntr,-zihintpause,-zihpm,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-zmmul,-zve32f,-zve32x,-zve64d,-zve64f,-zve64x,-zvfh,-zvl1024b,-zvl128b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl32b,-zvl4096b,-zvl512b,-zvl64b,-zvl65536b,-zvl8192b" }
attributes #1 = { nofree nounwind uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+c,+d,+f,+m,+relax,+zba,+zbb,+zbc,+zbs,+zicsr,+zifencei,-e,-experimental-smaia,-experimental-ssaia,-experimental-zacas,-experimental-zfa,-experimental-zfbfmin,-experimental-zicond,-experimental-zihintntl,-experimental-ztso,-experimental-zvbb,-experimental-zvbc,-experimental-zvfbfmin,-experimental-zvfbfwma,-experimental-zvkg,-experimental-zvkn,-experimental-zvknc,-experimental-zvkned,-experimental-zvkng,-experimental-zvknha,-experimental-zvknhb,-experimental-zvks,-experimental-zvksc,-experimental-zvksed,-experimental-zvksg,-experimental-zvksh,-experimental-zvkt,-h,-save-restore,-svinval,-svnapot,-svpbmt,-v,-xcvbitmanip,-xcvmac,-xsfcie,-xsfvcp,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-zawrs,-zbkb,-zbkc,-zbkx,-zca,-zcb,-zcd,-zce,-zcf,-zcmp,-zcmt,-zdinx,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zicbom,-zicbop,-zicboz,-zicntr,-zihintpause,-zihpm,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-zmmul,-zve32f,-zve32x,-zve64d,-zve64f,-zve64x,-zvfh,-zvl1024b,-zvl128b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl32b,-zvl4096b,-zvl512b,-zvl64b,-zvl65536b,-zvl8192b" }
attributes #2 = { mustprogress nofree nounwind willreturn "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic-rv64" "target-features"="+64bit,+a,+c,+d,+f,+m,+relax,+zba,+zbb,+zbc,+zbs,+zicsr,+zifencei,-e,-experimental-smaia,-experimental-ssaia,-experimental-zacas,-experimental-zfa,-experimental-zfbfmin,-experimental-zicond,-experimental-zihintntl,-experimental-ztso,-experimental-zvbb,-experimental-zvbc,-experimental-zvfbfmin,-experimental-zvfbfwma,-experimental-zvkg,-experimental-zvkn,-experimental-zvknc,-experimental-zvkned,-experimental-zvkng,-experimental-zvknha,-experimental-zvknhb,-experimental-zvks,-experimental-zvksc,-experimental-zvksed,-experimental-zvksg,-experimental-zvksh,-experimental-zvkt,-h,-save-restore,-svinval,-svnapot,-svpbmt,-v,-xcvbitmanip,-xcvmac,-xsfcie,-xsfvcp,-xtheadba,-xtheadbb,-xtheadbs,-xtheadcmo,-xtheadcondmov,-xtheadfmemidx,-xtheadmac,-xtheadmemidx,-xtheadmempair,-xtheadsync,-xtheadvdot,-xventanacondops,-zawrs,-zbkb,-zbkc,-zbkx,-zca,-zcb,-zcd,-zce,-zcf,-zcmp,-zcmt,-zdinx,-zfh,-zfhmin,-zfinx,-zhinx,-zhinxmin,-zicbom,-zicbop,-zicboz,-zicntr,-zihintpause,-zihpm,-zk,-zkn,-zknd,-zkne,-zknh,-zkr,-zks,-zksed,-zksh,-zkt,-zmmul,-zve32f,-zve32x,-zve64d,-zve64f,-zve64x,-zvfh,-zvl1024b,-zvl128b,-zvl16384b,-zvl2048b,-zvl256b,-zvl32768b,-zvl32b,-zvl4096b,-zvl512b,-zvl64b,-zvl65536b,-zvl8192b" }
attributes #3 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64d"}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 8, !"SmallDataLimit", i32 0}
!5 = !{!"Ubuntu clang version 17.0.6 (++20231209124227+6009708b4367-1~exp1~20231209124336.77)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = !{!9, !9, i64 0}
!9 = !{!"any pointer", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C/C++ TBAA"}
