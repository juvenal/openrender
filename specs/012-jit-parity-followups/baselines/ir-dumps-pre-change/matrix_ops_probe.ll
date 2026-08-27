; ModuleID = '/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups/shaders/matrix_ops_probe.slo'
source_filename = "matrix_ops_probe"

define void @matrix_ops_probe(i32 %0, ptr %1, ptr %2) {
entry:
  %globals_pp = getelementptr ptr, ptr %1, i32 1
  %globals = load ptr, ptr %globals_pp, align 8
  %locals_pp = getelementptr ptr, ptr %1, i32 2
  %locals = load ptr, ptr %locals_pp, align 8
  %numActive = alloca i32, align 4
  %numPassive = alloca i32, align 4
  store i32 %0, ptr %numActive, align 4
  store i32 0, ptr %numPassive, align 4
  %3 = getelementptr ptr, ptr %locals, i32 27
  %4 = load ptr, ptr %3, align 8
  %mfromf16_e = alloca [16 x ptr], align 8
  %mfromf16_se = alloca [16 x i32], align 4
  %5 = getelementptr ptr, ptr %locals, i32 0
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 0
  store ptr %6, ptr %7, align 8
  %8 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 0
  store i32 0, ptr %8, align 4
  %9 = getelementptr ptr, ptr %locals, i32 1
  %10 = load ptr, ptr %9, align 8
  %11 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 1
  store ptr %10, ptr %11, align 8
  %12 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 1
  store i32 0, ptr %12, align 4
  %13 = getelementptr ptr, ptr %locals, i32 2
  %14 = load ptr, ptr %13, align 8
  %15 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 2
  store ptr %14, ptr %15, align 8
  %16 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 2
  store i32 0, ptr %16, align 4
  %17 = getelementptr ptr, ptr %locals, i32 3
  %18 = load ptr, ptr %17, align 8
  %19 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 3
  store ptr %18, ptr %19, align 8
  %20 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 3
  store i32 0, ptr %20, align 4
  %21 = getelementptr ptr, ptr %locals, i32 4
  %22 = load ptr, ptr %21, align 8
  %23 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 4
  store ptr %22, ptr %23, align 8
  %24 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 4
  store i32 0, ptr %24, align 4
  %25 = getelementptr ptr, ptr %locals, i32 5
  %26 = load ptr, ptr %25, align 8
  %27 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 5
  store ptr %26, ptr %27, align 8
  %28 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 5
  store i32 0, ptr %28, align 4
  %29 = getelementptr ptr, ptr %locals, i32 6
  %30 = load ptr, ptr %29, align 8
  %31 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 6
  store ptr %30, ptr %31, align 8
  %32 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 6
  store i32 0, ptr %32, align 4
  %33 = getelementptr ptr, ptr %locals, i32 7
  %34 = load ptr, ptr %33, align 8
  %35 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 7
  store ptr %34, ptr %35, align 8
  %36 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 7
  store i32 0, ptr %36, align 4
  %37 = getelementptr ptr, ptr %locals, i32 8
  %38 = load ptr, ptr %37, align 8
  %39 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 8
  store ptr %38, ptr %39, align 8
  %40 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 8
  store i32 0, ptr %40, align 4
  %41 = getelementptr ptr, ptr %locals, i32 9
  %42 = load ptr, ptr %41, align 8
  %43 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 9
  store ptr %42, ptr %43, align 8
  %44 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 9
  store i32 0, ptr %44, align 4
  %45 = getelementptr ptr, ptr %locals, i32 10
  %46 = load ptr, ptr %45, align 8
  %47 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 10
  store ptr %46, ptr %47, align 8
  %48 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 10
  store i32 0, ptr %48, align 4
  %49 = getelementptr ptr, ptr %locals, i32 11
  %50 = load ptr, ptr %49, align 8
  %51 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 11
  store ptr %50, ptr %51, align 8
  %52 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 11
  store i32 0, ptr %52, align 4
  %53 = getelementptr ptr, ptr %locals, i32 12
  %54 = load ptr, ptr %53, align 8
  %55 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 12
  store ptr %54, ptr %55, align 8
  %56 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 12
  store i32 0, ptr %56, align 4
  %57 = getelementptr ptr, ptr %locals, i32 13
  %58 = load ptr, ptr %57, align 8
  %59 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 13
  store ptr %58, ptr %59, align 8
  %60 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 13
  store i32 0, ptr %60, align 4
  %61 = getelementptr ptr, ptr %locals, i32 14
  %62 = load ptr, ptr %61, align 8
  %63 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 14
  store ptr %62, ptr %63, align 8
  %64 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 14
  store i32 0, ptr %64, align 4
  %65 = getelementptr ptr, ptr %locals, i32 15
  %66 = load ptr, ptr %65, align 8
  %67 = getelementptr [16 x ptr], ptr %mfromf16_e, i32 0, i32 15
  store ptr %66, ptr %67, align 8
  %68 = getelementptr [16 x i32], ptr %mfromf16_se, i32 0, i32 15
  store i32 0, ptr %68, align 4
  call void @op_mfromf16(ptr %4, i32 0, ptr %mfromf16_e, ptr %mfromf16_se, i32 %0, ptr %2)
  %69 = getelementptr ptr, ptr %locals, i32 16
  %70 = load ptr, ptr %69, align 8
  %71 = getelementptr ptr, ptr %locals, i32 27
  %72 = load ptr, ptr %71, align 8
  call void @op_movemm(ptr %70, i32 0, ptr %72, i32 0, i32 %0, ptr %2)
  %73 = getelementptr ptr, ptr %locals, i32 28
  %74 = load ptr, ptr %73, align 8
  %75 = getelementptr ptr, ptr %locals, i32 0
  %76 = load ptr, ptr %75, align 8
  call void @op_moveff(ptr %74, i32 1, ptr %76, i32 0, i32 %0, ptr %2)
  %77 = getelementptr ptr, ptr %locals, i32 17
  %78 = load ptr, ptr %77, align 8
  %79 = getelementptr ptr, ptr %locals, i32 28
  %80 = load ptr, ptr %79, align 8
  %81 = getelementptr ptr, ptr %globals, i32 17
  %82 = load ptr, ptr %81, align 8
  call void @op_mulff(ptr %78, i32 1, ptr %80, i32 1, ptr %82, i32 1, i32 %0, ptr %2)
  %83 = getelementptr ptr, ptr %locals, i32 28
  %84 = load ptr, ptr %83, align 8
  %85 = getelementptr ptr, ptr %locals, i32 1
  %86 = load ptr, ptr %85, align 8
  call void @op_moveff(ptr %84, i32 1, ptr %86, i32 0, i32 %0, ptr %2)
  %87 = getelementptr ptr, ptr %locals, i32 29
  %88 = load ptr, ptr %87, align 8
  %89 = getelementptr ptr, ptr %locals, i32 2
  %90 = load ptr, ptr %89, align 8
  call void @op_moveff(ptr %88, i32 1, ptr %90, i32 0, i32 %0, ptr %2)
  %91 = getelementptr ptr, ptr %locals, i32 30
  %92 = load ptr, ptr %91, align 8
  %93 = getelementptr ptr, ptr %locals, i32 3
  %94 = load ptr, ptr %93, align 8
  call void @op_moveff(ptr %92, i32 1, ptr %94, i32 0, i32 %0, ptr %2)
  %95 = getelementptr ptr, ptr %locals, i32 31
  %96 = load ptr, ptr %95, align 8
  %97 = getelementptr ptr, ptr %locals, i32 4
  %98 = load ptr, ptr %97, align 8
  call void @op_moveff(ptr %96, i32 0, ptr %98, i32 0, i32 %0, ptr %2)
  %99 = getelementptr ptr, ptr %locals, i32 32
  %100 = load ptr, ptr %99, align 8
  %101 = getelementptr ptr, ptr %locals, i32 5
  %102 = load ptr, ptr %101, align 8
  call void @op_moveff(ptr %100, i32 0, ptr %102, i32 0, i32 %0, ptr %2)
  %103 = getelementptr ptr, ptr %locals, i32 33
  %104 = load ptr, ptr %103, align 8
  %105 = getelementptr ptr, ptr %locals, i32 6
  %106 = load ptr, ptr %105, align 8
  call void @op_moveff(ptr %104, i32 0, ptr %106, i32 0, i32 %0, ptr %2)
  %107 = getelementptr ptr, ptr %locals, i32 34
  %108 = load ptr, ptr %107, align 8
  %109 = getelementptr ptr, ptr %locals, i32 7
  %110 = load ptr, ptr %109, align 8
  call void @op_moveff(ptr %108, i32 0, ptr %110, i32 0, i32 %0, ptr %2)
  %111 = getelementptr ptr, ptr %locals, i32 35
  %112 = load ptr, ptr %111, align 8
  %113 = getelementptr ptr, ptr %locals, i32 8
  %114 = load ptr, ptr %113, align 8
  call void @op_moveff(ptr %112, i32 0, ptr %114, i32 0, i32 %0, ptr %2)
  %115 = getelementptr ptr, ptr %locals, i32 36
  %116 = load ptr, ptr %115, align 8
  %117 = getelementptr ptr, ptr %locals, i32 9
  %118 = load ptr, ptr %117, align 8
  call void @op_moveff(ptr %116, i32 0, ptr %118, i32 0, i32 %0, ptr %2)
  %119 = getelementptr ptr, ptr %locals, i32 37
  %120 = load ptr, ptr %119, align 8
  %121 = getelementptr ptr, ptr %locals, i32 10
  %122 = load ptr, ptr %121, align 8
  call void @op_moveff(ptr %120, i32 0, ptr %122, i32 0, i32 %0, ptr %2)
  %123 = getelementptr ptr, ptr %locals, i32 38
  %124 = load ptr, ptr %123, align 8
  %125 = getelementptr ptr, ptr %locals, i32 11
  %126 = load ptr, ptr %125, align 8
  call void @op_moveff(ptr %124, i32 0, ptr %126, i32 0, i32 %0, ptr %2)
  %127 = getelementptr ptr, ptr %locals, i32 39
  %128 = load ptr, ptr %127, align 8
  %129 = getelementptr ptr, ptr %locals, i32 12
  %130 = load ptr, ptr %129, align 8
  call void @op_moveff(ptr %128, i32 0, ptr %130, i32 0, i32 %0, ptr %2)
  %131 = getelementptr ptr, ptr %locals, i32 40
  %132 = load ptr, ptr %131, align 8
  %133 = getelementptr ptr, ptr %locals, i32 13
  %134 = load ptr, ptr %133, align 8
  call void @op_moveff(ptr %132, i32 0, ptr %134, i32 0, i32 %0, ptr %2)
  %135 = getelementptr ptr, ptr %locals, i32 41
  %136 = load ptr, ptr %135, align 8
  %137 = getelementptr ptr, ptr %locals, i32 14
  %138 = load ptr, ptr %137, align 8
  call void @op_moveff(ptr %136, i32 0, ptr %138, i32 0, i32 %0, ptr %2)
  %139 = getelementptr ptr, ptr %locals, i32 42
  %140 = load ptr, ptr %139, align 8
  %141 = getelementptr ptr, ptr %locals, i32 15
  %142 = load ptr, ptr %141, align 8
  call void @op_moveff(ptr %140, i32 0, ptr %142, i32 0, i32 %0, ptr %2)
  %143 = getelementptr ptr, ptr %locals, i32 18
  %144 = load ptr, ptr %143, align 8
  %mfromf16_e1 = alloca [16 x ptr], align 8
  %mfromf16_se2 = alloca [16 x i32], align 4
  %145 = getelementptr ptr, ptr %locals, i32 17
  %146 = load ptr, ptr %145, align 8
  %147 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 0
  store ptr %146, ptr %147, align 8
  %148 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 0
  store i32 1, ptr %148, align 4
  %149 = getelementptr ptr, ptr %locals, i32 28
  %150 = load ptr, ptr %149, align 8
  %151 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 1
  store ptr %150, ptr %151, align 8
  %152 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 1
  store i32 1, ptr %152, align 4
  %153 = getelementptr ptr, ptr %locals, i32 29
  %154 = load ptr, ptr %153, align 8
  %155 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 2
  store ptr %154, ptr %155, align 8
  %156 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 2
  store i32 1, ptr %156, align 4
  %157 = getelementptr ptr, ptr %locals, i32 30
  %158 = load ptr, ptr %157, align 8
  %159 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 3
  store ptr %158, ptr %159, align 8
  %160 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 3
  store i32 1, ptr %160, align 4
  %161 = getelementptr ptr, ptr %locals, i32 31
  %162 = load ptr, ptr %161, align 8
  %163 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 4
  store ptr %162, ptr %163, align 8
  %164 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 4
  store i32 0, ptr %164, align 4
  %165 = getelementptr ptr, ptr %locals, i32 32
  %166 = load ptr, ptr %165, align 8
  %167 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 5
  store ptr %166, ptr %167, align 8
  %168 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 5
  store i32 0, ptr %168, align 4
  %169 = getelementptr ptr, ptr %locals, i32 33
  %170 = load ptr, ptr %169, align 8
  %171 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 6
  store ptr %170, ptr %171, align 8
  %172 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 6
  store i32 0, ptr %172, align 4
  %173 = getelementptr ptr, ptr %locals, i32 34
  %174 = load ptr, ptr %173, align 8
  %175 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 7
  store ptr %174, ptr %175, align 8
  %176 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 7
  store i32 0, ptr %176, align 4
  %177 = getelementptr ptr, ptr %locals, i32 35
  %178 = load ptr, ptr %177, align 8
  %179 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 8
  store ptr %178, ptr %179, align 8
  %180 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 8
  store i32 0, ptr %180, align 4
  %181 = getelementptr ptr, ptr %locals, i32 36
  %182 = load ptr, ptr %181, align 8
  %183 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 9
  store ptr %182, ptr %183, align 8
  %184 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 9
  store i32 0, ptr %184, align 4
  %185 = getelementptr ptr, ptr %locals, i32 37
  %186 = load ptr, ptr %185, align 8
  %187 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 10
  store ptr %186, ptr %187, align 8
  %188 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 10
  store i32 0, ptr %188, align 4
  %189 = getelementptr ptr, ptr %locals, i32 38
  %190 = load ptr, ptr %189, align 8
  %191 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 11
  store ptr %190, ptr %191, align 8
  %192 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 11
  store i32 0, ptr %192, align 4
  %193 = getelementptr ptr, ptr %locals, i32 39
  %194 = load ptr, ptr %193, align 8
  %195 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 12
  store ptr %194, ptr %195, align 8
  %196 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 12
  store i32 0, ptr %196, align 4
  %197 = getelementptr ptr, ptr %locals, i32 40
  %198 = load ptr, ptr %197, align 8
  %199 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 13
  store ptr %198, ptr %199, align 8
  %200 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 13
  store i32 0, ptr %200, align 4
  %201 = getelementptr ptr, ptr %locals, i32 41
  %202 = load ptr, ptr %201, align 8
  %203 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 14
  store ptr %202, ptr %203, align 8
  %204 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 14
  store i32 0, ptr %204, align 4
  %205 = getelementptr ptr, ptr %locals, i32 42
  %206 = load ptr, ptr %205, align 8
  %207 = getelementptr [16 x ptr], ptr %mfromf16_e1, i32 0, i32 15
  store ptr %206, ptr %207, align 8
  %208 = getelementptr [16 x i32], ptr %mfromf16_se2, i32 0, i32 15
  store i32 0, ptr %208, align 4
  call void @op_mfromf16(ptr %144, i32 16, ptr %mfromf16_e1, ptr %mfromf16_se2, i32 %0, ptr %2)
  %209 = getelementptr ptr, ptr %locals, i32 19
  %210 = load ptr, ptr %209, align 8
  %211 = getelementptr ptr, ptr %locals, i32 16
  %212 = load ptr, ptr %211, align 8
  call void @op_movemm(ptr %210, i32 0, ptr %212, i32 0, i32 %0, ptr %2)
  %213 = getelementptr ptr, ptr %locals, i32 20
  %214 = load ptr, ptr %213, align 8
  %215 = getelementptr ptr, ptr %locals, i32 16
  %216 = load ptr, ptr %215, align 8
  %217 = getelementptr ptr, ptr %locals, i32 18
  %218 = load ptr, ptr %217, align 8
  call void @op_addmm(ptr %214, i32 16, ptr %216, i32 0, ptr %218, i32 16, i32 %0, ptr %2)
  %219 = getelementptr ptr, ptr %locals, i32 21
  %220 = load ptr, ptr %219, align 8
  %221 = getelementptr ptr, ptr %locals, i32 16
  %222 = load ptr, ptr %221, align 8
  %223 = getelementptr ptr, ptr %locals, i32 18
  %224 = load ptr, ptr %223, align 8
  call void @op_submm(ptr %220, i32 16, ptr %222, i32 0, ptr %224, i32 16, i32 %0, ptr %2)
  %225 = getelementptr ptr, ptr %locals, i32 22
  %226 = load ptr, ptr %225, align 8
  %227 = getelementptr ptr, ptr %locals, i32 16
  %228 = load ptr, ptr %227, align 8
  %229 = getelementptr ptr, ptr %locals, i32 18
  %230 = load ptr, ptr %229, align 8
  call void @op_divmm(ptr %226, i32 16, ptr %228, i32 0, ptr %230, i32 16, i32 %0, ptr %2)
  %231 = getelementptr ptr, ptr %locals, i32 23
  %232 = load ptr, ptr %231, align 8
  %233 = getelementptr ptr, ptr %locals, i32 16
  %234 = load ptr, ptr %233, align 8
  %235 = getelementptr ptr, ptr %locals, i32 18
  %236 = load ptr, ptr %235, align 8
  call void @op_mulmm(ptr %232, i32 16, ptr %234, i32 0, ptr %236, i32 16, i32 %0, ptr %2)
  %237 = getelementptr ptr, ptr %locals, i32 24
  %238 = load ptr, ptr %237, align 8
  %239 = getelementptr ptr, ptr %locals, i32 16
  %240 = load ptr, ptr %239, align 8
  call void @op_negm(ptr %238, i32 16, ptr %240, i32 0, i32 %0, ptr %2)
  %241 = getelementptr ptr, ptr %locals, i32 28
  %242 = load ptr, ptr %241, align 8
  %lit = alloca float, align 4
  store float 0.000000e+00, ptr %lit, align 4
  call void @op_moveff(ptr %242, i32 1, ptr %lit, i32 0, i32 %0, ptr %2)
  %243 = getelementptr ptr, ptr %locals, i32 25
  %244 = load ptr, ptr %243, align 8
  %245 = getelementptr ptr, ptr %globals, i32 17
  %246 = load ptr, ptr %245, align 8
  %247 = getelementptr ptr, ptr %globals, i32 18
  %248 = load ptr, ptr %247, align 8
  %249 = getelementptr ptr, ptr %locals, i32 28
  %250 = load ptr, ptr %249, align 8
  call void @op_vfromfff(ptr %244, i32 3, ptr %246, i32 1, ptr %248, i32 1, ptr %250, i32 1, i32 %0, ptr %2)
  %251 = getelementptr ptr, ptr %locals, i32 26
  %252 = load ptr, ptr %251, align 8
  %253 = getelementptr ptr, ptr %locals, i32 25
  %254 = load ptr, ptr %253, align 8
  call void @op_mfromv(ptr %252, i32 16, ptr %254, i32 3, i32 %0, ptr %2)
  %255 = getelementptr ptr, ptr %locals, i32 29
  %256 = load ptr, ptr %255, align 8
  %257 = getelementptr ptr, ptr %locals, i32 28
  %258 = load ptr, ptr %257, align 8
  call void @op_moveff(ptr %256, i32 1, ptr %258, i32 1, i32 %0, ptr %2)
  %259 = getelementptr ptr, ptr %locals, i32 30
  %260 = load ptr, ptr %259, align 8
  %261 = getelementptr ptr, ptr %locals, i32 28
  %262 = load ptr, ptr %261, align 8
  call void @op_moveff(ptr %260, i32 1, ptr %262, i32 1, i32 %0, ptr %2)
  %263 = getelementptr ptr, ptr %locals, i32 28
  %264 = load ptr, ptr %263, align 8
  %265 = getelementptr ptr, ptr %locals, i32 48
  %266 = load ptr, ptr %265, align 8
  %267 = getelementptr ptr, ptr %locals, i32 28
  %268 = load ptr, ptr %267, align 8
  call void @op_vfromf(ptr %266, i32 3, ptr %268, i32 1, i32 %0, ptr %2)
  %269 = getelementptr ptr, ptr %locals, i32 47
  %270 = load ptr, ptr %269, align 8
  %271 = getelementptr ptr, ptr %globals, i32 7
  %272 = load ptr, ptr %271, align 8
  %273 = getelementptr ptr, ptr %locals, i32 48
  %274 = load ptr, ptr %273, align 8
  call void @op_mulvv(ptr %270, i32 3, ptr %272, i32 3, ptr %274, i32 3, i32 %0, ptr %2)
  %275 = getelementptr ptr, ptr %locals, i32 29
  %276 = load ptr, ptr %275, align 8
  %lit3 = alloca float, align 4
  store float 0.000000e+00, ptr %lit3, align 4
  call void @op_moveff(ptr %276, i32 1, ptr %lit3, i32 0, i32 %0, ptr %2)
  %277 = getelementptr ptr, ptr %locals, i32 30
  %278 = load ptr, ptr %277, align 8
  %279 = getelementptr ptr, ptr %locals, i32 29
  %280 = load ptr, ptr %279, align 8
  call void @op_moveff(ptr %278, i32 1, ptr %280, i32 1, i32 %0, ptr %2)
  %281 = getelementptr ptr, ptr %locals, i32 28
  %282 = load ptr, ptr %281, align 8
  %283 = getelementptr ptr, ptr %locals, i32 48
  %284 = load ptr, ptr %283, align 8
  %285 = getelementptr ptr, ptr %locals, i32 28
  %286 = load ptr, ptr %285, align 8
  call void @op_vfromf(ptr %284, i32 3, ptr %286, i32 1, i32 %0, ptr %2)
  %287 = getelementptr ptr, ptr %locals, i32 46
  %288 = load ptr, ptr %287, align 8
  %289 = getelementptr ptr, ptr %locals, i32 47
  %290 = load ptr, ptr %289, align 8
  %291 = getelementptr ptr, ptr %locals, i32 48
  %292 = load ptr, ptr %291, align 8
  call void @op_mulvv(ptr %288, i32 3, ptr %290, i32 3, ptr %292, i32 3, i32 %0, ptr %2)
  %293 = getelementptr ptr, ptr %locals, i32 29
  %294 = load ptr, ptr %293, align 8
  %lit4 = alloca float, align 4
  store float 0.000000e+00, ptr %lit4, align 4
  call void @op_moveff(ptr %294, i32 1, ptr %lit4, i32 0, i32 %0, ptr %2)
  %295 = getelementptr ptr, ptr %locals, i32 30
  %296 = load ptr, ptr %295, align 8
  %297 = getelementptr ptr, ptr %locals, i32 29
  %298 = load ptr, ptr %297, align 8
  call void @op_moveff(ptr %296, i32 1, ptr %298, i32 1, i32 %0, ptr %2)
  %299 = getelementptr ptr, ptr %locals, i32 28
  %300 = load ptr, ptr %299, align 8
  %301 = getelementptr ptr, ptr %locals, i32 47
  %302 = load ptr, ptr %301, align 8
  %303 = getelementptr ptr, ptr %locals, i32 28
  %304 = load ptr, ptr %303, align 8
  call void @op_vfromf(ptr %302, i32 3, ptr %304, i32 1, i32 %0, ptr %2)
  %305 = getelementptr ptr, ptr %locals, i32 45
  %306 = load ptr, ptr %305, align 8
  %307 = getelementptr ptr, ptr %locals, i32 46
  %308 = load ptr, ptr %307, align 8
  %309 = getelementptr ptr, ptr %locals, i32 47
  %310 = load ptr, ptr %309, align 8
  call void @op_mulvv(ptr %306, i32 3, ptr %308, i32 3, ptr %310, i32 3, i32 %0, ptr %2)
  %311 = getelementptr ptr, ptr %locals, i32 29
  %312 = load ptr, ptr %311, align 8
  %lit5 = alloca float, align 4
  store float 0.000000e+00, ptr %lit5, align 4
  call void @op_moveff(ptr %312, i32 1, ptr %lit5, i32 0, i32 %0, ptr %2)
  %313 = getelementptr ptr, ptr %locals, i32 30
  %314 = load ptr, ptr %313, align 8
  %315 = getelementptr ptr, ptr %locals, i32 29
  %316 = load ptr, ptr %315, align 8
  call void @op_moveff(ptr %314, i32 1, ptr %316, i32 1, i32 %0, ptr %2)
  %317 = getelementptr ptr, ptr %locals, i32 28
  %318 = load ptr, ptr %317, align 8
  %319 = getelementptr ptr, ptr %locals, i32 46
  %320 = load ptr, ptr %319, align 8
  %321 = getelementptr ptr, ptr %locals, i32 28
  %322 = load ptr, ptr %321, align 8
  call void @op_vfromf(ptr %320, i32 3, ptr %322, i32 1, i32 %0, ptr %2)
  %323 = getelementptr ptr, ptr %locals, i32 44
  %324 = load ptr, ptr %323, align 8
  %325 = getelementptr ptr, ptr %locals, i32 45
  %326 = load ptr, ptr %325, align 8
  %327 = getelementptr ptr, ptr %locals, i32 46
  %328 = load ptr, ptr %327, align 8
  call void @op_mulvv(ptr %324, i32 3, ptr %326, i32 3, ptr %328, i32 3, i32 %0, ptr %2)
  %329 = getelementptr ptr, ptr %locals, i32 29
  %330 = load ptr, ptr %329, align 8
  %lit6 = alloca float, align 4
  store float 0.000000e+00, ptr %lit6, align 4
  call void @op_moveff(ptr %330, i32 1, ptr %lit6, i32 0, i32 %0, ptr %2)
  %331 = getelementptr ptr, ptr %locals, i32 30
  %332 = load ptr, ptr %331, align 8
  %333 = getelementptr ptr, ptr %locals, i32 29
  %334 = load ptr, ptr %333, align 8
  call void @op_moveff(ptr %332, i32 1, ptr %334, i32 1, i32 %0, ptr %2)
  %335 = getelementptr ptr, ptr %locals, i32 28
  %336 = load ptr, ptr %335, align 8
  %337 = getelementptr ptr, ptr %locals, i32 45
  %338 = load ptr, ptr %337, align 8
  %339 = getelementptr ptr, ptr %locals, i32 28
  %340 = load ptr, ptr %339, align 8
  call void @op_vfromf(ptr %338, i32 3, ptr %340, i32 1, i32 %0, ptr %2)
  %341 = getelementptr ptr, ptr %locals, i32 43
  %342 = load ptr, ptr %341, align 8
  %343 = getelementptr ptr, ptr %locals, i32 44
  %344 = load ptr, ptr %343, align 8
  %345 = getelementptr ptr, ptr %locals, i32 45
  %346 = load ptr, ptr %345, align 8
  call void @op_mulvv(ptr %342, i32 3, ptr %344, i32 3, ptr %346, i32 3, i32 %0, ptr %2)
  %347 = getelementptr ptr, ptr %locals, i32 29
  %348 = load ptr, ptr %347, align 8
  %lit7 = alloca float, align 4
  store float 0.000000e+00, ptr %lit7, align 4
  call void @op_moveff(ptr %348, i32 1, ptr %lit7, i32 0, i32 %0, ptr %2)
  %349 = getelementptr ptr, ptr %locals, i32 30
  %350 = load ptr, ptr %349, align 8
  %351 = getelementptr ptr, ptr %locals, i32 29
  %352 = load ptr, ptr %351, align 8
  call void @op_moveff(ptr %350, i32 1, ptr %352, i32 1, i32 %0, ptr %2)
  %353 = getelementptr ptr, ptr %locals, i32 28
  %354 = load ptr, ptr %353, align 8
  %355 = getelementptr ptr, ptr %locals, i32 44
  %356 = load ptr, ptr %355, align 8
  %357 = getelementptr ptr, ptr %locals, i32 28
  %358 = load ptr, ptr %357, align 8
  call void @op_vfromf(ptr %356, i32 3, ptr %358, i32 1, i32 %0, ptr %2)
  %359 = getelementptr ptr, ptr %globals, i32 11
  %360 = load ptr, ptr %359, align 8
  %361 = getelementptr ptr, ptr %locals, i32 43
  %362 = load ptr, ptr %361, align 8
  %363 = getelementptr ptr, ptr %locals, i32 44
  %364 = load ptr, ptr %363, align 8
  call void @op_mulvv(ptr %360, i32 3, ptr %362, i32 3, ptr %364, i32 3, i32 %0, ptr %2)
  ret void
}

declare void @op_mfromf16(ptr, i32, ptr, ptr, i32, ptr)

declare void @op_movemm(ptr, i32, ptr, i32, i32, ptr)

declare void @op_moveff(ptr, i32, ptr, i32, i32, ptr)

declare void @op_mulff(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_addmm(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_submm(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_divmm(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_mulmm(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_negm(ptr, i32, ptr, i32, i32, ptr)

declare void @op_vfromfff(ptr, i32, ptr, i32, ptr, i32, ptr, i32, i32, ptr)

declare void @op_mfromv(ptr, i32, ptr, i32, i32, ptr)

declare void @op_vfromf(ptr, i32, ptr, i32, i32, ptr)

declare void @op_mulvv(ptr, i32, ptr, i32, ptr, i32, i32, ptr)

!openrender.shader.name = !{!0}
!openrender.shader.type = !{!1}
!openrender.shader.version = !{!2}
!openrender.shader.usedparameters = !{!3}
!openrender.shader.params = !{!4, !5, !6, !7, !8, !9, !10, !11, !12, !13, !14, !15, !16, !17, !18, !19}
!openrender.shader.vars = !{!20, !21, !22, !23, !24, !25, !26, !27, !28, !29, !30, !31, !32, !33, !34, !35, !36, !37, !38, !39, !40, !41, !42, !43, !44, !45, !46, !47, !48, !49, !50, !51, !52}

!0 = !{!"matrix_ops_probe"}
!1 = !{!"surface"}
!2 = !{!"1.0.0"}
!3 = !{!"134217727"}
!4 = !{!"f00", !"float", !"uniform", !"false", !"1", !"1"}
!5 = !{!"f01", !"float", !"uniform", !"false", !"1", !"0"}
!6 = !{!"f02", !"float", !"uniform", !"false", !"1", !"0"}
!7 = !{!"f03", !"float", !"uniform", !"false", !"1", !"0"}
!8 = !{!"f10", !"float", !"uniform", !"false", !"1", !"0"}
!9 = !{!"f11", !"float", !"uniform", !"false", !"1", !"1"}
!10 = !{!"f12", !"float", !"uniform", !"false", !"1", !"0"}
!11 = !{!"f13", !"float", !"uniform", !"false", !"1", !"0"}
!12 = !{!"f20", !"float", !"uniform", !"false", !"1", !"0"}
!13 = !{!"f21", !"float", !"uniform", !"false", !"1", !"0"}
!14 = !{!"f22", !"float", !"uniform", !"false", !"1", !"1"}
!15 = !{!"f23", !"float", !"uniform", !"false", !"1", !"0"}
!16 = !{!"f30", !"float", !"uniform", !"false", !"1", !"0"}
!17 = !{!"f31", !"float", !"uniform", !"false", !"1", !"0"}
!18 = !{!"f32", !"float", !"uniform", !"false", !"1", !"0"}
!19 = !{!"f33", !"float", !"uniform", !"false", !"1", !"1"}
!20 = !{!"M1", !"matrix", !"uniform", !"false", !"1", !""}
!21 = !{!"vf00", !"float", !"varying", !"false", !"1", !""}
!22 = !{!"M2", !"matrix", !"varying", !"false", !"1", !""}
!23 = !{!"M3", !"matrix", !"uniform", !"false", !"1", !""}
!24 = !{!"Msum", !"matrix", !"varying", !"false", !"1", !""}
!25 = !{!"Mdiff", !"matrix", !"varying", !"false", !"1", !""}
!26 = !{!"Mdiv", !"matrix", !"varying", !"false", !"1", !""}
!27 = !{!"Mmul", !"matrix", !"varying", !"false", !"1", !""}
!28 = !{!"Mneg", !"matrix", !"varying", !"false", !"1", !""}
!29 = !{!"Vv", !"vector", !"varying", !"false", !"1", !""}
!30 = !{!"M4", !"matrix", !"varying", !"false", !"1", !""}
!31 = !{!"temporary_0", !"matrix", !"uniform", !"false", !"1", !""}
!32 = !{!"temporary_1", !"float", !"varying", !"false", !"1", !""}
!33 = !{!"temporary_2", !"float", !"varying", !"false", !"1", !""}
!34 = !{!"temporary_3", !"float", !"varying", !"false", !"1", !""}
!35 = !{!"temporary_4", !"float", !"uniform", !"false", !"1", !""}
!36 = !{!"temporary_5", !"float", !"uniform", !"false", !"1", !""}
!37 = !{!"temporary_6", !"float", !"uniform", !"false", !"1", !""}
!38 = !{!"temporary_7", !"float", !"uniform", !"false", !"1", !""}
!39 = !{!"temporary_8", !"float", !"uniform", !"false", !"1", !""}
!40 = !{!"temporary_9", !"float", !"uniform", !"false", !"1", !""}
!41 = !{!"temporary_10", !"float", !"uniform", !"false", !"1", !""}
!42 = !{!"temporary_11", !"float", !"uniform", !"false", !"1", !""}
!43 = !{!"temporary_12", !"float", !"uniform", !"false", !"1", !""}
!44 = !{!"temporary_13", !"float", !"uniform", !"false", !"1", !""}
!45 = !{!"temporary_14", !"float", !"uniform", !"false", !"1", !""}
!46 = !{!"temporary_15", !"float", !"uniform", !"false", !"1", !""}
!47 = !{!"temporary_16", !"vector", !"varying", !"false", !"1", !""}
!48 = !{!"temporary_17", !"vector", !"varying", !"false", !"1", !""}
!49 = !{!"temporary_18", !"vector", !"varying", !"false", !"1", !""}
!50 = !{!"temporary_19", !"vector", !"varying", !"false", !"1", !""}
!51 = !{!"temporary_20", !"vector", !"varying", !"false", !"1", !""}
!52 = !{!"temporary_21", !"vector", !"varying", !"false", !"1", !""}
