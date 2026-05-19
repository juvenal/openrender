file(GLOB oshader_headers *.h)
set(oshader_sources
	oshader.cpp
#	../ri/dso.cpp
	expression.cpp
	opcodes.cpp
	rslo.cpp
#	rslo.cpp
	# Preprocessor: pp1.c–pp8.c combined into one TU via the EXTERN/MAIN
	# convention described in pp_all.c.  Individual pp*.c files are NOT
	# listed here; they are included transitively from pp_all.c.
	pp_all.c
	# Layer 2 IR infrastructure
	ir.cpp
	irBuilder.cpp
	rsloEmitter.cpp
	passes/passManager.cpp
	passes/passDCE.cpp
	passes/passUniformLifting.cpp
	passes/passConstFold.cpp
	passes/passCSE.cpp)

include_directories(AFTER ../includes)

if(USE_FLEX_BISON)
	# this is to make sure generated bison cpp's include the correct lex.xxx.cpp
	include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
	include_directories(AFTER .)
	include_directories(AFTER ../ri)

	# define flex targets
	flex_file(rslo.l lex.rslo.cpp rslo)
	flex_file(../ri/dso.l lex.dso.cpp dso)

	# define bison targets (they depend on flex ones, via prefix - third arg)
	bison_file(rslo.y rslo_generated.cpp rslo)
	bison_file(../ri/dso.y dso_generated.cpp dso)

	# add the generated bison cpp's
	set(oshader_sources ${oshader_sources} rslo_generated.cpp dso_generated.cpp)
else()
	# add the ones from the source tree
	set(oshader_sources ${oshader_sources} rslo.cpp ../ri/dso.cpp)
endif()

add_executable(oshader ${oshader_sources})

# Require C17 for C files and C++20 for C++ files
target_compile_features(oshader PRIVATE c_std_17 cxx_std_20)

# Strict warning flags for clean, standard-conforming code
# -Wpedantic is CXX-only: GCC C17 emits pedantic errors at every zero-arg LOG macro
# call site (##__VA_ARGS__ GNU extension), blocking the build. GCC C++20 does not
# warn about ##__VA_ARGS__, so C++ files are clean with -Wpedantic.
# C-only flags are gated to avoid GCC "valid for C/ObjC but not C++" warnings.
target_compile_options(oshader PRIVATE
	-Wall
	-Wextra
	-Wimplicit-fallthrough
	-Wmissing-declarations
	-Wnull-dereference
	-Wshadow
	$<$<COMPILE_LANGUAGE:C>:
		-Wold-style-definition
		-Wstrict-prototypes
		-Wmissing-prototypes>
	$<$<COMPILE_LANGUAGE:CXX>:
		-Wpedantic
		-Wsuggest-override>
)

target_link_libraries(oshader openrendercommon)
install(TARGETS oshader RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
