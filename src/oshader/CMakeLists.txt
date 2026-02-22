file(GLOB oshader_headers *.h)
set(oshader_sources
	oshader.cpp
#	../ri/dso.cpp
	expression.cpp
	opcodes.cpp
	sdr.cpp
#	sl.cpp
	pp1.c
	pp2.c
	pp3.c
	pp4.c
	pp5.c
	pp6.c
	pp7.c
	pp8.c)

include_directories(AFTER ../includes)

if(USE_FLEX_BISON)
	# this is to make sure generated bison cpp's include the correct lex.xxx.cpp
	include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
	include_directories(AFTER .)
	include_directories(AFTER ../ri)

	# define flex targets
	flex_file(sl.l lex.sl.cpp sl)
	flex_file(../ri/dso.l lex.dso.cpp dso)

	# define bison targets (they depend on flex ones, via prefix - third arg)
	bison_file(sl.y sl_generated.cpp sl)
	bison_file(../ri/dso.y dso_generated.cpp dso)

	# add the generated bison cpp's
	set(oshader_sources ${oshader_sources} sl_generated.cpp dso_generated.cpp)
else()
	# add the ones from the source tree
	set(oshader_sources ${oshader_sources} sl.cpp ../ri/dso.cpp)
endif()

add_executable(oshader ${oshader_sources})

# Require C17 for C files and C++20 for C++ files
target_compile_features(oshader PRIVATE c_std_17 cxx_std_20)

# Strict warning flags for clean, standard-conforming code
target_compile_options(oshader PRIVATE
	-Wall
	-Wextra
	-Wpedantic
	-Wold-style-definition
	-Wstrict-prototypes
	-Wmissing-prototypes
	-Wmissing-declarations
	-Wnull-dereference
	-Wshadow
	$<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>
)

target_link_libraries(oshader openrendercommon)
install(TARGETS oshader RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
