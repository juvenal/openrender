# Configure CPack for FHS builds (RPM and DEB packages)
if(NOT INSTALL_SELFCONTAINED)
    set(CPACK_PACKAGE_NAME "openrender")
    set(CPACK_PACKAGE_VENDOR "openRender Team")
    set(CPACK_PACKAGE_CONTACT "cedric PAILLE <cedric.paille@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "3D renderer Renderman compliant")
    set(CPACK_PACKAGE_DESCRIPTION "openRender is a RenderMan like photorealistic renderer. It is being developed in the hope that it will be useful for graphics research and for people who can not afford a commercial renderer.")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    
    # Versioning
    set(CPACK_PACKAGE_VERSION_MAJOR "${OPENRENDER_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${OPENRENDER_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${OPENRENDER_VERSION_PATCH}")
    
    # Formulate clean SemVer version without brackets for package managers
    set(_cpack_ver "${OPENRENDER_VERSION}")
    if(OPENRENDER_VERSION_PRERELEASE)
        set(_cpack_ver "${_cpack_ver}-${OPENRENDER_VERSION_PRERELEASE}")
    endif()
    if(OPENRENDER_VERSION_BUILD_METADATA)
        set(_cpack_ver "${_cpack_ver}+${OPENRENDER_VERSION_BUILD_METADATA}")
    endif()
    set(CPACK_PACKAGE_VERSION "${_cpack_ver}")
    
    set(CPACK_GENERATOR "DEB;RPM")
    
    # Debian specific settings
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "cedric PAILLE <cedric.paille@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    # Automatically compute shared library dependencies (like libtiff, libpng, wayland, etc.)
    set(CPACK_DEBIAN_PACKAGE_SHLIBS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_FILE_NAME "openrender-${_cpack_ver}-x86_64-ubuntu.deb")
    
    # RPM specific settings
    set(CPACK_RPM_PACKAGE_LICENSE "LGPL-2.1-only")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
    # Automatically compute dependencies for RPM
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_FILE_NAME "openrender-${_cpack_ver}-x86_64-fedora.rpm")
    
    include(CPack)
endif()
