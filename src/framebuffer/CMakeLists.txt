file(GLOB framebuffer_headers *.h)

# Check for Wayland dependencies (Linux only)
if(UNIX AND NOT APPLE)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(WAYLAND wayland-client wayland-protocols>=1.24)
	if(WAYLAND_FOUND)
		# Generate Wayland protocol headers
		set(XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")
		set(FRACTIONAL_SCALE_XML "/usr/share/wayland-protocols/staging/fractional-scale/fractional-scale-v1.xml")
		
		add_custom_command(
			OUTPUT xdg-shell-client-protocol.h
			COMMAND wayland-scanner client-header ${XDG_SHELL_XML} xdg-shell-client-protocol.h
			DEPENDS ${XDG_SHELL_XML}
		)
		add_custom_command(
			OUTPUT xdg-shell-protocol.c
			COMMAND wayland-scanner private-code ${XDG_SHELL_XML} xdg-shell-protocol.c
			DEPENDS ${XDG_SHELL_XML}
		)
		add_custom_command(
			OUTPUT fractional-scale-v1-client-protocol.h
			COMMAND wayland-scanner client-header ${FRACTIONAL_SCALE_XML} fractional-scale-v1-client-protocol.h
			DEPENDS ${FRACTIONAL_SCALE_XML}
		)
		add_custom_command(
			OUTPUT fractional-scale-v1-protocol.c
			COMMAND wayland-scanner private-code ${FRACTIONAL_SCALE_XML} fractional-scale-v1-protocol.c
			DEPENDS ${FRACTIONAL_SCALE_XML}
		)

	endif(WAYLAND_FOUND)

	# libdecor: cross-compositor window decoration (CSD on GNOME, SSD on KDE/Weston)
	pkg_check_modules(LIBDECOR libdecor-0)
	if(LIBDECOR_FOUND)
		message(STATUS "Found libdecor: window decorations enabled for Wayland")
	else()
		message(STATUS "libdecor not found — Wayland windows will be undecorated on GNOME")
	endif()
endif(UNIX AND NOT APPLE)

# Framebuffer display module — platform-specific
if(WIN32)
	add_library(framebuffer MODULE framebuffer.cpp fbw.cpp ${framebuffer_headers})
	target_link_libraries(framebuffer openrendercommon)

elseif(UNIX AND NOT APPLE)
	# Linux: X11 base + optional Wayland
	find_package(X11 REQUIRED)
	if(X11_FOUND)
		set(FB_SRCS framebuffer.cpp fbx.cpp ${framebuffer_headers})
		set(FB_LIBS ${X11_LIBRARIES})
		set(FB_INCS ${X11_INCLUDE_DIR})
		set(FB_DEFS "")

		if(WAYLAND_FOUND)
			message(STATUS "Integrating Wayland support into framebuffer module")
			list(APPEND FB_SRCS fbwl.cpp xdg-shell-protocol.c xdg-shell-client-protocol.h fractional-scale-v1-protocol.c fractional-scale-v1-client-protocol.h)
			list(APPEND FB_LIBS ${WAYLAND_LIBRARIES})
			list(APPEND FB_INCS ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
			list(APPEND FB_DEFS HAVE_WAYLAND)
			if(LIBDECOR_FOUND)
				list(APPEND FB_LIBS ${LIBDECOR_LIBRARIES})
				list(APPEND FB_INCS ${LIBDECOR_INCLUDE_DIRS})
				list(APPEND FB_DEFS HAVE_LIBDECOR)
			endif()
		endif()

		add_library(framebuffer MODULE ${FB_SRCS})
		target_include_directories(framebuffer PRIVATE ${FB_INCS})
		target_link_libraries(framebuffer openrendercommon ${FB_LIBS} pthread)
		target_compile_definitions(framebuffer PRIVATE ${FB_DEFS})
	else()
		message(FATAL_ERROR "Could not find X11 libs, cannot build framebuffer.so")
	endif()

elseif(APPLE)
	# macOS: Quartz/Cocoa driver — not yet implemented (planned as fbq.cpp)
	message(STATUS "macOS framebuffer display driver not yet implemented (planned: Quartz/Cocoa)")

endif()

if(TARGET framebuffer)
	set_target_properties(framebuffer PROPERTIES PREFIX "")
	install(TARGETS framebuffer LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
endif()

# Standalone Wayland module (optional, for explicit use)
if(WAYLAND_FOUND)
	set(FBWL_SRCS
		framebuffer.cpp
		fbwl.cpp
		${framebuffer_headers}
		xdg-shell-client-protocol.h
		xdg-shell-protocol.c
		fractional-scale-v1-client-protocol.h
		fractional-scale-v1-protocol.c
	)
	set(FBWL_DEFS HAVE_WAYLAND)
	if(LIBDECOR_FOUND)
		list(APPEND FBWL_DEFS HAVE_LIBDECOR)
	endif()
	add_library(fbwl MODULE ${FBWL_SRCS})
	set(FBWL_INCS ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
	set(FBWL_LIBS ${WAYLAND_LIBRARIES})
	if(LIBDECOR_FOUND)
		list(APPEND FBWL_INCS ${LIBDECOR_INCLUDE_DIRS})
		list(APPEND FBWL_LIBS ${LIBDECOR_LIBRARIES})
	endif()
	target_include_directories(fbwl PRIVATE ${FBWL_INCS})
	target_link_libraries(fbwl openrendercommon ${FBWL_LIBS} pthread)
	set_target_properties(fbwl PROPERTIES PREFIX "")
	install(TARGETS fbwl LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
	target_compile_definitions(fbwl PRIVATE ${FBWL_DEFS})
endif(WAYLAND_FOUND)
