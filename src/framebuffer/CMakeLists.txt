file(GLOB framebuffer_headers *.h)

# Framebuffer display module — platform-specific
if(WIN32)
	add_library(framebuffer MODULE framebuffer.cpp fbw.cpp ${framebuffer_headers})
	target_link_libraries(framebuffer openrendercommon)

elseif(UNIX AND NOT APPLE)
	# Linux: IPC framebuffer driver — spawns orender-fb-linux helper for display.
	# The helper handles X11 and Wayland window backends internally.
	find_package(X11 REQUIRED)
	if(X11_FOUND)
		add_library(framebuffer MODULE framebuffer.cpp fbx.cpp ${framebuffer_headers})
		target_link_libraries(framebuffer openrendercommon pthread)
		target_compile_features(framebuffer PRIVATE cxx_std_20)
		message(STATUS "Linux framebuffer display driver: fbx.cpp (IPC) + orender-fb-linux helper")
	else()
		message(FATAL_ERROR "Could not find X11 libs, cannot build framebuffer.so")
	endif()

	# Build the helper binary (has its own X11 + Wayland + libdecor deps)
	find_package(PkgConfig REQUIRED)
	add_subdirectory(orender-fb-linux)

elseif(APPLE)
	# macOS: IPC driver (fbq.cpp) — spawns orender-fb-macos helper
	add_library(framebuffer MODULE framebuffer.cpp fbq.cpp ${framebuffer_headers})
	target_link_libraries(framebuffer openrendercommon)
	target_compile_features(framebuffer PRIVATE cxx_std_20)
	add_subdirectory(orender-fb-macos)
	message(STATUS "macOS framebuffer display driver: fbq.cpp + orender-fb-macos helper")

endif()

if(TARGET framebuffer)
	set_target_properties(framebuffer PROPERTIES PREFIX "")
	install(TARGETS framebuffer LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
endif()
