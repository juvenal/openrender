; ModuleID = '/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups/shaders/show_ctransform.slo'
source_filename = "show_ctransform"

@space_str = private unnamed_addr constant [4 x i8] c"hsv\00", align 1

define void @show_ctransform(i32 %0, ptr %1, ptr %2) {
entry:
  %globals_pp = getelementptr ptr, ptr %1, i32 1
  %globals = load ptr, ptr %globals_pp, align 8
  %locals_pp = getelementptr ptr, ptr %1, i32 2
  %locals = load ptr, ptr %locals_pp, align 8
  %numActive = alloca i32, align 4
  %numPassive = alloca i32, align 4
  store i32 %0, ptr %numActive, align 4
  store i32 0, ptr %numPassive, align 4
  %3 = getelementptr ptr, ptr %locals, i32 1
  %4 = load ptr, ptr %3, align 8
  %lit = alloca float, align 4
  store float 0.000000e+00, ptr %lit, align 4
  call void @op_moveff(ptr %4, i32 0, ptr %lit, i32 0, i32 %0, ptr %2)
  %5 = getelementptr ptr, ptr %locals, i32 0
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr ptr, ptr %globals, i32 13
  %8 = load ptr, ptr %7, align 8
  %9 = getelementptr ptr, ptr %globals, i32 14
  %10 = load ptr, ptr %9, align 8
  %11 = getelementptr ptr, ptr %locals, i32 1
  %12 = load ptr, ptr %11, align 8
  call void @op_vfromfff(ptr %6, i32 3, ptr %8, i32 1, ptr %10, i32 1, ptr %12, i32 0, i32 %0, ptr %2)
  %13 = getelementptr ptr, ptr %globals, i32 11
  %14 = load ptr, ptr %13, align 8
  %15 = getelementptr ptr, ptr %locals, i32 0
  %16 = load ptr, ptr %15, align 8
  call void @op_ctransform(ptr %14, i32 3, ptr @space_str, ptr %16, i32 3, i32 %0, ptr %2)
  %17 = getelementptr ptr, ptr %locals, i32 2
  %18 = load ptr, ptr %17, align 8
  %lit1 = alloca float, align 4
  store float 1.000000e+00, ptr %lit1, align 4
  call void @op_vfromf(ptr %18, i32 0, ptr %lit1, i32 0, i32 %0, ptr %2)
  %19 = getelementptr ptr, ptr %globals, i32 12
  %20 = load ptr, ptr %19, align 8
  %21 = getelementptr ptr, ptr %locals, i32 2
  %22 = load ptr, ptr %21, align 8
  call void @op_movevv(ptr %20, i32 3, ptr %22, i32 0, i32 %0, ptr %2)
  ret void
}

declare void @op_moveff(ptr, i32, ptr, i32, i32, ptr)

declare void @op_vfromfff(ptr, i32, ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_ctransform(ptr, i32, ptr, ptr, i32, i32, ptr)

declare void @op_vfromf(ptr, i32, ptr, i32, i32, ptr)

declare void @op_movevv(ptr, i32, ptr, i32, i32, ptr)

!openrender.shader.name = !{!0}
!openrender.shader.type = !{!1}
!openrender.shader.version = !{!2}
!openrender.shader.usedparameters = !{!3}
!openrender.shader.params = !{}
!openrender.shader.vars = !{!4, !5, !6}

!0 = !{!"show_ctransform"}
!1 = !{!"surface"}
!2 = !{!"1.0.0"}
!3 = !{!"134217727"}
!4 = !{!"temporary_0", !"vector", !"varying", !"false", !"1", !""}
!5 = !{!"temporary_1", !"float", !"uniform", !"false", !"1", !""}
!6 = !{!"temporary_2", !"vector", !"uniform", !"false", !"1", !""}
