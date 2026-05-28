file(GLOB framebuffer_headers *.h)

# Framebuffer display module — platform-specific
if(WIN32)
	# Windows: native GDI driver, no IPC
	add_library(framebuffer MODULE framebuffer.cpp fbw.cpp ${framebuffer_headers})
	target_link_libraries(framebuffer openrendercommon)

else()
	# macOS + Linux: unified IPC driver — spawns orender-fb helper for display.
	# The driver has no X11/Wayland/Cocoa dependency; all windowing lives in the helper.
	add_library(framebuffer MODULE framebuffer.cpp fbipc_display.cpp ${framebuffer_headers})
	target_link_libraries(framebuffer openrendercommon pthread)
	target_compile_features(framebuffer PRIVATE cxx_std_20)

	if(UNIX AND NOT APPLE)
		# Build the Linux helper (has its own X11 + Wayland + libdecor deps)
		find_package(PkgConfig REQUIRED)
		add_subdirectory(orender-fb-linux)
		message(STATUS "Linux framebuffer display driver: fbipc_display.cpp (IPC) + orender-fb-linux helper")
	elseif(APPLE)
		add_subdirectory(orender-fb-macos)
		message(STATUS "macOS framebuffer display driver: fbipc_display.cpp (IPC) + orender-fb-macos helper")
	endif()

endif()

if(TARGET framebuffer)
	set_target_properties(framebuffer PROPERTIES PREFIX "" SUFFIX ".dsply")
	install(TARGETS framebuffer LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
endif()
