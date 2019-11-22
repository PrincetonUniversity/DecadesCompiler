; ModuleID = 'decades_base/main_visited.cpp'
source_filename = "decades_base/main_visited.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, i8* }
%union.anon = type { i32 }

$__clang_call_terminate = comdat any

@.str = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@0 = private unnamed_addr global %struct.ident_t { i32 0, i32 34, i32 0, i32 0, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str, i32 0, i32 0) }, align 8
@1 = private unnamed_addr global %struct.ident_t { i32 0, i32 2, i32 0, i32 0, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str, i32 0, i32 0) }, align 8
@.str.2 = private unnamed_addr constant [22 x i8] c"reference result: %f\0A\00", align 1
@str = private unnamed_addr constant [10 x i8] c"finished!\00", align 1

; Function Attrs: noinline uwtable
define dso_local void @DECADES_BARRIER() local_unnamed_addr #0 {
entry:
  %0 = tail call i32 @__kmpc_global_thread_num(%struct.ident_t* nonnull @1)
  tail call void @__kmpc_barrier(%struct.ident_t* nonnull @0, i32 %0)
  ret void
}

declare dso_local i32 @__kmpc_global_thread_num(%struct.ident_t*) local_unnamed_addr

declare dso_local void @__kmpc_barrier(%struct.ident_t*, i32) local_unnamed_addr

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local i32 @DECADES_COMPARE_EXCHANGE_STRONG(i32* nocapture %addr, i32* nocapture %expected, i32 %desired) local_unnamed_addr #1 {
entry:
  %0 = load i32, i32* %expected, align 4
  %1 = cmpxchg i32* %addr, i32 %0, i32 %desired monotonic monotonic
  %2 = extractvalue { i32, i1 } %1, 1
  br i1 %2, label %cmpxchg.continue, label %cmpxchg.store_expected

cmpxchg.store_expected:                           ; preds = %entry
  %3 = extractvalue { i32, i1 } %1, 0
  store i32 %3, i32* %expected, align 4
  br label %cmpxchg.continue

cmpxchg.continue:                                 ; preds = %cmpxchg.store_expected, %entry
  %conv = zext i1 %2 to i32
  ret i32 %conv
}

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local i32 @DECADES_COMPARE_EXCHANGE_WEAK(i32* nocapture %addr, i32* nocapture %expected, i32 %desired) local_unnamed_addr #1 {
entry:
  %0 = load i32, i32* %expected, align 4
  %1 = cmpxchg weak i32* %addr, i32 %0, i32 %desired monotonic monotonic
  %2 = extractvalue { i32, i1 } %1, 1
  br i1 %2, label %cmpxchg.continue, label %cmpxchg.store_expected

cmpxchg.store_expected:                           ; preds = %entry
  %3 = extractvalue { i32, i1 } %1, 0
  store i32 %3, i32* %expected, align 4
  br label %cmpxchg.continue

cmpxchg.continue:                                 ; preds = %cmpxchg.store_expected, %entry
  %conv = zext i1 %2 to i32
  ret i32 %conv
}

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local i32 @DECADES_COMPARE_AND_SWAP(i32* %addr, i32 %to_compare, i32 %new_val) local_unnamed_addr #1 {
entry:
  %to_compare.addr = alloca i32, align 4
  store i32 %to_compare, i32* %to_compare.addr, align 4, !tbaa !2
  %0 = load volatile i32, i32* %addr, align 4, !tbaa !2
  %call = call i32 @DECADES_COMPARE_EXCHANGE_STRONG(i32* %addr, i32* nonnull %to_compare.addr, i32 %new_val)
  ret i32 %0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local i32 @DECADES_FETCH_ADD(i32* nocapture %addr, i32 %to_add) local_unnamed_addr #1 {
entry:
  %0 = atomicrmw add i32* %addr, i32 %to_add monotonic
  ret i32 %0
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @DECADES_FETCH_MIN(i32* %addr, i32 %to_min) local_unnamed_addr #3 {
entry:
  %value = alloca i32, align 4
  %0 = bitcast i32* %value to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #9
  %1 = load volatile i32, i32* %addr, align 4, !tbaa !2
  store i32 %1, i32* %value, align 4, !tbaa !2
  %cmp5 = icmp sgt i32 %1, %to_min
  br i1 %cmp5, label %while.body, label %cleanup

while.body:                                       ; preds = %entry, %while.body.while.cond_crit_edge
  %call = call i32 @DECADES_COMPARE_EXCHANGE_WEAK(i32* %addr, i32* nonnull %value, i32 %to_min)
  %tobool = icmp eq i32 %call, 0
  br i1 %tobool, label %while.body.while.cond_crit_edge, label %cleanup

while.body.while.cond_crit_edge:                  ; preds = %while.body
  %.pre = load i32, i32* %value, align 4, !tbaa !2
  %cmp = icmp sgt i32 %.pre, %to_min
  br i1 %cmp, label %while.body, label %cleanup

cleanup:                                          ; preds = %while.body, %while.body.while.cond_crit_edge, %entry
  %retval.0 = phi i32 [ 0, %entry ], [ 1, %while.body ], [ 0, %while.body.while.cond_crit_edge ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #9
  ret i32 %retval.0
}

; Function Attrs: noinline nounwind uwtable
define dso_local float @DECADES_FETCH_ADD_FLOAT(float* %addr, float %to_add) local_unnamed_addr #3 {
entry:
  %value = alloca %union.anon, align 4
  %0 = bitcast %union.anon* %value to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #9
  %float_val = bitcast %union.anon* %value to float*
  %1 = bitcast float* %addr to i32*
  %int_val = getelementptr inbounds %union.anon, %union.anon* %value, i64 0, i32 0
  br label %do.body

do.body:                                          ; preds = %do.body, %entry
  %2 = load volatile float, float* %addr, align 4, !tbaa !6
  store float %2, float* %float_val, align 4, !tbaa !8
  %add = fadd float %2, %to_add
  %3 = bitcast float %add to i32
  %call = call i32 @DECADES_COMPARE_EXCHANGE_WEAK(i32* %1, i32* nonnull %int_val, i32 %3)
  %tobool = icmp eq i32 %call, 0
  br i1 %tobool, label %do.body, label %do.end

do.end:                                           ; preds = %do.body
  %4 = load float, float* %float_val, align 4, !tbaa !8
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #9
  ret float %4
}

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local void @_Z15_kernel_computePfS_S_ii(float* nocapture readonly %a, float* nocapture readonly %b, float* nocapture %c, i32 %tid, i32 %num_threads) local_unnamed_addr #1 {
entry:
  %cmp12 = icmp slt i32 %tid, 256
  br i1 %cmp12, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %entry
  %0 = sext i32 %tid to i64
  %1 = sext i32 %num_threads to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ %0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %arrayidx = getelementptr inbounds float, float* %a, i64 %indvars.iv
  %2 = load float, float* %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds float, float* %b, i64 %indvars.iv
  %3 = load float, float* %arrayidx2, align 4, !tbaa !6
  %add = fadd float %2, %3
  %arrayidx4 = getelementptr inbounds float, float* %c, i64 %indvars.iv
  %4 = load float, float* %arrayidx4, align 4, !tbaa !6
  %add5 = fadd float %4, %add
  store float %add5, float* %arrayidx4, align 4, !tbaa !6
  %indvars.iv.next = add i64 %indvars.iv, %1
  %cmp = icmp slt i64 %indvars.iv.next, 256
  br i1 %cmp, label %for.body, label %for.cond.cleanup
}

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local void @_Z14_kernel_supplyPfS_S_ii(float* nocapture readonly %a, float* nocapture readonly %b, float* nocapture %c, i32 %tid, i32 %num_threads) local_unnamed_addr #1 {
entry:
  %cmp12 = icmp slt i32 %tid, 256
  br i1 %cmp12, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %entry
  %0 = sext i32 %tid to i64
  %1 = sext i32 %num_threads to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ %0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %arrayidx = getelementptr inbounds float, float* %a, i64 %indvars.iv
  %2 = load float, float* %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds float, float* %b, i64 %indvars.iv
  %3 = load float, float* %arrayidx2, align 4, !tbaa !6
  %add = fadd float %2, %3
  %arrayidx4 = getelementptr inbounds float, float* %c, i64 %indvars.iv
  %4 = load float, float* %arrayidx4, align 4, !tbaa !6
  %add5 = fadd float %4, %add
  store float %add5, float* %arrayidx4, align 4, !tbaa !6
  %indvars.iv.next = add i64 %indvars.iv, %1
  %cmp = icmp slt i64 %indvars.iv.next, 256
  br i1 %cmp, label %for.body, label %for.cond.cleanup
}

; Function Attrs: noinline norecurse nounwind uwtable
define dso_local void @_Z8_kernel_PfS_S_ii(float* nocapture readonly %a, float* nocapture readonly %b, float* nocapture %c, i32 %tid, i32 %num_threads) local_unnamed_addr #1 {
entry:
  %cmp12 = icmp slt i32 %tid, 256
  br i1 %cmp12, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %entry
  %0 = sext i32 %tid to i64
  %1 = sext i32 %num_threads to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ %0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %arrayidx = getelementptr inbounds float, float* %a, i64 %indvars.iv
  %2 = load float, float* %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds float, float* %b, i64 %indvars.iv
  %3 = load float, float* %arrayidx2, align 4, !tbaa !6
  %add = fadd float %2, %3
  %arrayidx4 = getelementptr inbounds float, float* %c, i64 %indvars.iv
  %4 = load float, float* %arrayidx4, align 4, !tbaa !6
  %add5 = fadd float %4, %add
  store float %add5, float* %arrayidx4, align 4, !tbaa !6
  %indvars.iv.next = add i64 %indvars.iv, %1
  %cmp = icmp slt i64 %indvars.iv.next, 256
  br i1 %cmp, label %for.body, label %for.cond.cleanup
}

; Function Attrs: norecurse uwtable
define dso_local i32 @main() local_unnamed_addr #4 {
entry:
  %a = alloca [256 x float], align 16
  %b = alloca [256 x float], align 16
  %c = alloca [256 x float], align 16
  %0 = bitcast [256 x float]* %c to i8*
  %1 = bitcast [256 x float]* %a to i8*
  call void @llvm.lifetime.start.p0i8(i64 1024, i8* nonnull %1) #9
  %2 = bitcast [256 x float]* %b to i8*
  call void @llvm.lifetime.start.p0i8(i64 1024, i8* nonnull %2) #9
  call void @llvm.lifetime.start.p0i8(i64 1024, i8* nonnull %0) #9
  call void @llvm.memset.p0i8.i64(i8* nonnull align 16 %0, i8 0, i64 1024, i1 false)
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  tail call void @omp_set_dynamic(i32 0)
  tail call void @omp_set_num_threads(i32 16)
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* nonnull @1, i32 3, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, [256 x float]*, [256 x float]*, [256 x float]*)* @.omp_outlined. to void (i32*, i32*, ...)*), [256 x float]* nonnull %a, [256 x float]* nonnull %b, [256 x float]* nonnull %c)
  %puts = call i32 @puts(i8* getelementptr inbounds ([10 x i8], [10 x i8]* @str, i64 0, i64 0))
  %arrayidx12 = getelementptr inbounds [256 x float], [256 x float]* %c, i64 0, i64 0
  %.pre = load float, float* %arrayidx12, align 16, !tbaa !6
  br label %for.body9

for.body:                                         ; preds = %for.body, %entry
  %indvars.iv30 = phi i64 [ 0, %entry ], [ %indvars.iv.next31, %for.body ]
  %3 = trunc i64 %indvars.iv30 to i32
  %conv = sitofp i32 %3 to float
  %arrayidx = getelementptr inbounds [256 x float], [256 x float]* %b, i64 0, i64 %indvars.iv30
  store float %conv, float* %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds [256 x float], [256 x float]* %a, i64 0, i64 %indvars.iv30
  store float %conv, float* %arrayidx2, align 4, !tbaa !6
  %indvars.iv.next31 = add nuw nsw i64 %indvars.iv30, 1
  %exitcond32 = icmp eq i64 %indvars.iv.next31, 256
  br i1 %exitcond32, label %for.cond.cleanup, label %for.body

for.cond.cleanup8:                                ; preds = %for.body9
  %conv17 = fpext float %add to double
  %call18 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([22 x i8], [22 x i8]* @.str.2, i64 0, i64 0), double %conv17)
  call void @llvm.lifetime.end.p0i8(i64 1024, i8* nonnull %0) #9
  call void @llvm.lifetime.end.p0i8(i64 1024, i8* nonnull %2) #9
  call void @llvm.lifetime.end.p0i8(i64 1024, i8* nonnull %1) #9
  ret i32 0

for.body9:                                        ; preds = %for.body9, %for.cond.cleanup
  %4 = phi float [ %.pre, %for.cond.cleanup ], [ %add, %for.body9 ]
  %indvars.iv = phi i64 [ 1, %for.cond.cleanup ], [ %indvars.iv.next, %for.body9 ]
  %arrayidx11 = getelementptr inbounds [256 x float], [256 x float]* %c, i64 0, i64 %indvars.iv
  %5 = load float, float* %arrayidx11, align 4, !tbaa !6
  %add = fadd float %5, %4
  store float %add, float* %arrayidx12, align 16, !tbaa !6
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, 256
  br i1 %exitcond, label %for.cond.cleanup8, label %for.body9
}

declare dso_local void @omp_set_dynamic(i32) local_unnamed_addr #5

declare dso_local void @omp_set_num_threads(i32) local_unnamed_addr #5

; Function Attrs: norecurse nounwind uwtable
define internal void @.omp_outlined.(i32* noalias nocapture readnone %.global_tid., i32* noalias nocapture readnone %.bound_tid., [256 x float]* nocapture readonly dereferenceable(1024) %a, [256 x float]* nocapture readonly dereferenceable(1024) %b, [256 x float]* nocapture dereferenceable(1024) %c) #6 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %call = invoke i32 @omp_get_thread_num()
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %entry
  %arraydecay = getelementptr inbounds [256 x float], [256 x float]* %a, i64 0, i64 0
  %arraydecay1 = getelementptr inbounds [256 x float], [256 x float]* %b, i64 0, i64 0
  %arraydecay2 = getelementptr inbounds [256 x float], [256 x float]* %c, i64 0, i64 0
  tail call void @_Z15_kernel_computePfS_S_ii(float* nonnull %arraydecay, float* nonnull %arraydecay1, float* nonnull %arraydecay2, i32 %call, i32 16)
  ret void

lpad:                                             ; preds = %entry
  %0 = landingpad { i8*, i32 }
          catch i8* null
  %1 = extractvalue { i8*, i32 } %0, 0
  tail call void @__clang_call_terminate(i8* %1) #10
  unreachable
}

declare dso_local i32 @omp_get_thread_num() local_unnamed_addr #5

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8*) local_unnamed_addr #7 comdat {
  %2 = tail call i8* @__cxa_begin_catch(i8* %0) #9
  tail call void @_ZSt9terminatev() #10
  unreachable
}

declare dso_local i8* @__cxa_begin_catch(i8*) local_unnamed_addr

declare dso_local void @_ZSt9terminatev() local_unnamed_addr

declare !callback !9 dso_local void @__kmpc_fork_call(%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) local_unnamed_addr

; Function Attrs: nounwind
declare dso_local i32 @printf(i8* nocapture readonly, ...) local_unnamed_addr #8

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #9

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #2

attributes #0 = { noinline uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { noinline norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { norecurse uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #6 = { norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #7 = { noinline noreturn nounwind }
attributes #8 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #9 = { nounwind }
attributes #10 = { noreturn nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (/home/ts20/share/llvm9/llvm-project/clang 3775794812e582769e2ed1b53c00650a6b21387c)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C++ TBAA"}
!6 = !{!7, !7, i64 0}
!7 = !{!"float", !4, i64 0}
!8 = !{!4, !4, i64 0}
!9 = !{!10}
!10 = !{i64 2, i64 -1, i64 -1, i1 true}
