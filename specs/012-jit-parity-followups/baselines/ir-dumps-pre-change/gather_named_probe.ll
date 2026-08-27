; ModuleID = '/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups/shaders/gather_named_probe.slo'
source_filename = "gather_named_probe"

@gh_name = private unnamed_addr constant [5 x i8] c"bias\00", align 1
@gh_name.1 = private unnamed_addr constant [8 x i8] c"maxdist\00", align 1
@gh_name.2 = private unnamed_addr constant [11 x i8] c"samplebase\00", align 1
@gh_strval = private unnamed_addr constant [7 x i8] c"cosine\00", align 1
@gh_name.3 = private unnamed_addr constant [13 x i8] c"distribution\00", align 1
@gh_strval.4 = private unnamed_addr constant [9 x i8] c"myGather\00", align 1
@gh_name.5 = private unnamed_addr constant [6 x i8] c"label\00", align 1
@gh_name.6 = private unnamed_addr constant [11 x i8] c"surface:Ci\00", align 1
@gh_name.7 = private unnamed_addr constant [11 x i8] c"ray:origin\00", align 1
@gh_name.8 = private unnamed_addr constant [14 x i8] c"ray:direction\00", align 1
@gh_name.9 = private unnamed_addr constant [11 x i8] c"ray:length\00", align 1

define void @gather_named_probe(i32 %0, ptr %1, ptr %2) {
entry:
  %globals_pp = getelementptr ptr, ptr %1, i32 1
  %globals = load ptr, ptr %globals_pp, align 8
  %locals_pp = getelementptr ptr, ptr %1, i32 2
  %locals = load ptr, ptr %locals_pp, align 8
  %numActive = alloca i32, align 4
  %numPassive = alloca i32, align 4
  store i32 %0, ptr %numActive, align 4
  store i32 0, ptr %numPassive, align 4
  %3 = getelementptr ptr, ptr %locals, i32 0
  %4 = load ptr, ptr %3, align 8
  %5 = getelementptr ptr, ptr %globals, i32 2
  %6 = load ptr, ptr %5, align 8
  call void @op_normalize(ptr %4, i32 3, ptr %6, i32 3, i32 %0, ptr %2)
  %7 = getelementptr ptr, ptr %locals, i32 7
  %8 = load ptr, ptr %7, align 8
  %lit = alloca float, align 4
  store float 0.000000e+00, ptr %lit, align 4
  call void @op_vfromf(ptr %8, i32 0, ptr %lit, i32 0, i32 %0, ptr %2)
  %9 = getelementptr ptr, ptr %locals, i32 1
  %10 = load ptr, ptr %9, align 8
  %11 = getelementptr ptr, ptr %locals, i32 7
  %12 = load ptr, ptr %11, align 8
  call void @op_movevv(ptr %10, i32 3, ptr %12, i32 0, i32 %0, ptr %2)
  %13 = getelementptr ptr, ptr %locals, i32 2
  %14 = load ptr, ptr %13, align 8
  %lit1 = alloca float, align 4
  store float 8.000000e+00, ptr %lit1, align 4
  call void @op_moveff(ptr %14, i32 0, ptr %lit1, i32 0, i32 %0, ptr %2)
  %15 = getelementptr ptr, ptr %locals, i32 7
  %16 = load ptr, ptr %15, align 8
  %lit2 = alloca float, align 4
  store float 0.000000e+00, ptr %lit2, align 4
  call void @op_vfromf(ptr %16, i32 0, ptr %lit2, i32 0, i32 %0, ptr %2)
  %17 = getelementptr ptr, ptr %locals, i32 3
  %18 = load ptr, ptr %17, align 8
  %19 = getelementptr ptr, ptr %locals, i32 7
  %20 = load ptr, ptr %19, align 8
  call void @op_movevv(ptr %18, i32 3, ptr %20, i32 0, i32 %0, ptr %2)
  %21 = getelementptr ptr, ptr %locals, i32 7
  %22 = load ptr, ptr %21, align 8
  %lit3 = alloca float, align 4
  store float 0.000000e+00, ptr %lit3, align 4
  call void @op_vfromf(ptr %22, i32 0, ptr %lit3, i32 0, i32 %0, ptr %2)
  %23 = getelementptr ptr, ptr %locals, i32 4
  %24 = load ptr, ptr %23, align 8
  %25 = getelementptr ptr, ptr %locals, i32 7
  %26 = load ptr, ptr %25, align 8
  call void @op_movevv(ptr %24, i32 3, ptr %26, i32 0, i32 %0, ptr %2)
  %27 = getelementptr ptr, ptr %locals, i32 5
  %28 = load ptr, ptr %27, align 8
  %lit4 = alloca float, align 4
  store float 0.000000e+00, ptr %lit4, align 4
  call void @op_moveff(ptr %28, i32 1, ptr %lit4, i32 0, i32 %0, ptr %2)
  %29 = getelementptr ptr, ptr %locals, i32 7
  %30 = load ptr, ptr %29, align 8
  %lit5 = alloca float, align 4
  store float 0.000000e+00, ptr %lit5, align 4
  call void @op_vfromf(ptr %30, i32 0, ptr %lit5, i32 0, i32 %0, ptr %2)
  %31 = getelementptr ptr, ptr %locals, i32 6
  %32 = load ptr, ptr %31, align 8
  %33 = getelementptr ptr, ptr %locals, i32 7
  %34 = load ptr, ptr %33, align 8
  call void @op_movevv(ptr %32, i32 3, ptr %34, i32 0, i32 %0, ptr %2)
  %35 = getelementptr ptr, ptr %locals, i32 8
  %36 = load ptr, ptr %35, align 8
  %37 = getelementptr ptr, ptr %locals, i32 10
  %38 = load ptr, ptr %37, align 8
  %lit6 = alloca float, align 4
  store float 0x3FF921FB60000000, ptr %lit6, align 4
  call void @op_moveff(ptr %38, i32 0, ptr %lit6, i32 0, i32 %0, ptr %2)
  %39 = getelementptr ptr, ptr %locals, i32 9
  %40 = load ptr, ptr %39, align 8
  %41 = getelementptr ptr, ptr %locals, i32 10
  %42 = load ptr, ptr %41, align 8
  call void @op_moveff(ptr %40, i32 0, ptr %42, i32 0, i32 %0, ptr %2)
  %43 = getelementptr ptr, ptr %globals, i32 0
  %44 = load ptr, ptr %43, align 8
  %45 = getelementptr ptr, ptr %locals, i32 0
  %46 = load ptr, ptr %45, align 8
  %47 = getelementptr ptr, ptr %locals, i32 9
  %48 = load ptr, ptr %47, align 8
  %49 = getelementptr ptr, ptr %locals, i32 2
  %50 = load ptr, ptr %49, align 8
  %51 = load float, ptr %50, align 4
  %gh_names = alloca [9 x ptr], align 8
  %gh_values = alloca [9 x ptr], align 8
  %gh_steps = alloca [9 x i32], align 4
  %gh_varying = alloca [9 x i32], align 4
  %lit7 = alloca float, align 4
  store float 0x3F847AE140000000, ptr %lit7, align 4
  %52 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 0
  store ptr @gh_name, ptr %52, align 8
  %53 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 0
  store ptr %lit7, ptr %53, align 8
  %54 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 0
  store i32 4, ptr %54, align 4
  %55 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 0
  store i32 0, ptr %55, align 4
  %lit8 = alloca float, align 4
  store float 1.000000e+03, ptr %lit8, align 4
  %56 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 1
  store ptr @gh_name.1, ptr %56, align 8
  %57 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 1
  store ptr %lit8, ptr %57, align 8
  %58 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 1
  store i32 4, ptr %58, align 4
  %59 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 1
  store i32 0, ptr %59, align 4
  %lit9 = alloca float, align 4
  store float 2.000000e+00, ptr %lit9, align 4
  %60 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 2
  store ptr @gh_name.2, ptr %60, align 8
  %61 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 2
  store ptr %lit9, ptr %61, align 8
  %62 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 2
  store i32 4, ptr %62, align 4
  %63 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 2
  store i32 0, ptr %63, align 4
  %gh_strval_pp = alloca ptr, align 8
  store ptr @gh_strval, ptr %gh_strval_pp, align 8
  %64 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 3
  store ptr @gh_name.3, ptr %64, align 8
  %65 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 3
  store ptr %gh_strval_pp, ptr %65, align 8
  %66 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 3
  store i32 8, ptr %66, align 4
  %67 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 3
  store i32 0, ptr %67, align 4
  %gh_strval_pp10 = alloca ptr, align 8
  store ptr @gh_strval.4, ptr %gh_strval_pp10, align 8
  %68 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 4
  store ptr @gh_name.5, ptr %68, align 8
  %69 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 4
  store ptr %gh_strval_pp10, ptr %69, align 8
  %70 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 4
  store i32 8, ptr %70, align 4
  %71 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 4
  store i32 0, ptr %71, align 4
  %72 = getelementptr ptr, ptr %locals, i32 1
  %73 = load ptr, ptr %72, align 8
  %74 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 5
  store ptr @gh_name.6, ptr %74, align 8
  %75 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 5
  store ptr %73, ptr %75, align 8
  %76 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 5
  store i32 0, ptr %76, align 4
  %77 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 5
  store i32 0, ptr %77, align 4
  %78 = getelementptr ptr, ptr %locals, i32 3
  %79 = load ptr, ptr %78, align 8
  %80 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 6
  store ptr @gh_name.7, ptr %80, align 8
  %81 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 6
  store ptr %79, ptr %81, align 8
  %82 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 6
  store i32 0, ptr %82, align 4
  %83 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 6
  store i32 0, ptr %83, align 4
  %84 = getelementptr ptr, ptr %locals, i32 4
  %85 = load ptr, ptr %84, align 8
  %86 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 7
  store ptr @gh_name.8, ptr %86, align 8
  %87 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 7
  store ptr %85, ptr %87, align 8
  %88 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 7
  store i32 0, ptr %88, align 4
  %89 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 7
  store i32 0, ptr %89, align 4
  %90 = getelementptr ptr, ptr %locals, i32 5
  %91 = load ptr, ptr %90, align 8
  %92 = getelementptr [9 x ptr], ptr %gh_names, i32 0, i32 8
  store ptr @gh_name.9, ptr %92, align 8
  %93 = getelementptr [9 x ptr], ptr %gh_values, i32 0, i32 8
  store ptr %91, ptr %93, align 8
  %94 = getelementptr [9 x i32], ptr %gh_steps, i32 0, i32 8
  store i32 0, ptr %94, align 4
  %95 = getelementptr [9 x i32], ptr %gh_varying, i32 0, i32 8
  store i32 0, ptr %95, align 4
  call void @op_gatherHeader(ptr %gh_names, ptr %gh_values, ptr %gh_steps, ptr %gh_varying, i32 9, ptr %44, i32 3, ptr %46, i32 3, ptr %48, i32 0, float %51)
  br label %gather.header

gather.header:                                    ; preds = %gather.header, %entry
  %96 = call i32 @op_gather_begin(ptr %numActive, ptr %numPassive)
  %97 = getelementptr ptr, ptr %locals, i32 6
  %98 = load ptr, ptr %97, align 8
  %99 = getelementptr ptr, ptr %locals, i32 6
  %100 = load ptr, ptr %99, align 8
  %101 = getelementptr ptr, ptr %locals, i32 1
  %102 = load ptr, ptr %101, align 8
  call void @op_addvv(ptr %98, i32 3, ptr %100, i32 3, ptr %102, i32 3, i32 %0, ptr %2)
  %103 = call i32 @op_gather_else(ptr %numActive, ptr %numPassive)
  %104 = getelementptr ptr, ptr %locals, i32 7
  %105 = load ptr, ptr %104, align 8
  %lit11 = alloca float, align 4
  store float 0.000000e+00, ptr %lit11, align 4
  call void @op_vfromf(ptr %105, i32 0, ptr %lit11, i32 0, i32 %0, ptr %2)
  %106 = getelementptr ptr, ptr %locals, i32 11
  %107 = load ptr, ptr %106, align 8
  %108 = getelementptr ptr, ptr %locals, i32 7
  %109 = load ptr, ptr %108, align 8
  call void @op_movevv(ptr %107, i32 3, ptr %109, i32 0, i32 %0, ptr %2)
  %110 = getelementptr ptr, ptr %locals, i32 6
  %111 = load ptr, ptr %110, align 8
  %112 = getelementptr ptr, ptr %locals, i32 6
  %113 = load ptr, ptr %112, align 8
  %114 = getelementptr ptr, ptr %locals, i32 11
  %115 = load ptr, ptr %114, align 8
  call void @op_addvv(ptr %111, i32 3, ptr %113, i32 3, ptr %115, i32 3, i32 %0, ptr %2)
  %116 = call i32 @op_gather_end(ptr %numActive, ptr %numPassive)
  %117 = icmp ne i32 %116, 0
  br i1 %117, label %gather.header, label %gather.exit

gather.exit:                                      ; preds = %gather.header
  %118 = getelementptr ptr, ptr %locals, i32 7
  %119 = load ptr, ptr %118, align 8
  %lit12 = alloca float, align 4
  store float 0x3FA99999A0000000, ptr %lit12, align 4
  call void @op_vfromf(ptr %119, i32 0, ptr %lit12, i32 0, i32 %0, ptr %2)
  %120 = getelementptr ptr, ptr %locals, i32 11
  %121 = load ptr, ptr %120, align 8
  %122 = getelementptr ptr, ptr %locals, i32 7
  %123 = load ptr, ptr %122, align 8
  call void @op_movevv(ptr %121, i32 3, ptr %123, i32 0, i32 %0, ptr %2)
  %124 = getelementptr ptr, ptr %locals, i32 13
  %125 = load ptr, ptr %124, align 8
  %126 = getelementptr ptr, ptr %locals, i32 2
  %127 = load ptr, ptr %126, align 8
  call void @op_vfromf(ptr %125, i32 0, ptr %127, i32 0, i32 %0, ptr %2)
  %128 = getelementptr ptr, ptr %locals, i32 12
  %129 = load ptr, ptr %128, align 8
  %130 = getelementptr ptr, ptr %locals, i32 6
  %131 = load ptr, ptr %130, align 8
  %132 = getelementptr ptr, ptr %locals, i32 13
  %133 = load ptr, ptr %132, align 8
  call void @op_divvv(ptr %129, i32 3, ptr %131, i32 3, ptr %133, i32 0, i32 %0, ptr %2)
  %134 = getelementptr ptr, ptr %globals, i32 11
  %135 = load ptr, ptr %134, align 8
  %136 = getelementptr ptr, ptr %locals, i32 11
  %137 = load ptr, ptr %136, align 8
  %138 = getelementptr ptr, ptr %locals, i32 12
  %139 = load ptr, ptr %138, align 8
  call void @op_addvv(ptr %135, i32 3, ptr %137, i32 3, ptr %139, i32 3, i32 %0, ptr %2)
  %140 = getelementptr ptr, ptr %locals, i32 7
  %141 = load ptr, ptr %140, align 8
  %lit13 = alloca float, align 4
  store float 1.000000e+00, ptr %lit13, align 4
  call void @op_vfromf(ptr %141, i32 0, ptr %lit13, i32 0, i32 %0, ptr %2)
  %142 = getelementptr ptr, ptr %globals, i32 12
  %143 = load ptr, ptr %142, align 8
  %144 = getelementptr ptr, ptr %locals, i32 7
  %145 = load ptr, ptr %144, align 8
  call void @op_movevv(ptr %143, i32 3, ptr %145, i32 0, i32 %0, ptr %2)
  ret void
}

declare void @op_normalize(ptr, i32, ptr, i32, i32, ptr)

declare void @op_vfromf(ptr, i32, ptr, i32, i32, ptr)

declare void @op_movevv(ptr, i32, ptr, i32, i32, ptr)

declare void @op_moveff(ptr, i32, ptr, i32, i32, ptr)

declare void @op_gatherHeader(ptr, ptr, ptr, ptr, i32, ptr, i32, ptr, i32, ptr, i32, float)

declare i32 @op_gather_begin(ptr, ptr)

declare void @op_addvv(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare i32 @op_gather_else(ptr, ptr)

declare i32 @op_gather_end(ptr, ptr)

declare void @op_divvv(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

!openrender.shader.name = !{!0}
!openrender.shader.type = !{!1}
!openrender.shader.version = !{!2}
!openrender.shader.usedparameters = !{!3}
!openrender.shader.params = !{}
!openrender.shader.vars = !{!4, !5, !6, !7, !8, !9, !10, !11, !12, !13, !14, !15, !16, !17}

!0 = !{!"gather_named_probe"}
!1 = !{!"surface"}
!2 = !{!"1.0.0"}
!3 = !{!"134217727"}
!4 = !{!"Nn", !"normal", !"varying", !"false", !"1", !""}
!5 = !{!"Csum", !"color", !"varying", !"false", !"1", !""}
!6 = !{!"samples", !"float", !"uniform", !"false", !"1", !""}
!7 = !{!"rOrigin", !"point", !"varying", !"false", !"1", !""}
!8 = !{!"rDir", !"vector", !"varying", !"false", !"1", !""}
!9 = !{!"rLen", !"float", !"varying", !"false", !"1", !""}
!10 = !{!"Ctotal", !"color", !"varying", !"false", !"1", !""}
!11 = !{!"temporary_0", !"vector", !"uniform", !"false", !"1", !""}
!12 = !{!"temporary_1", !"string", !"uniform", !"false", !"1", !""}
!13 = !{!"temporary_2", !"float", !"uniform", !"false", !"1", !""}
!14 = !{!"temporary_3", !"float", !"uniform", !"false", !"1", !""}
!15 = !{!"temporary_4", !"vector", !"varying", !"false", !"1", !""}
!16 = !{!"temporary_5", !"vector", !"varying", !"false", !"1", !""}
!17 = !{!"temporary_6", !"vector", !"uniform", !"false", !"1", !""}
