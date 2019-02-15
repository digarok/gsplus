
INCLUDE (CheckFunctionExists)
INCLUDE (CheckLibraryExists)
INCLUDE (CheckTypeSize)
INCLUDE (CheckIncludeFile)
INCLUDE (CheckCSourceCompiles)

INCLUDE (FindPkgConfig)
INCLUDE (TestBigEndian)


set(PACKAGE_NAME "GSplus")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)


if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	add_compile_options(
		-O2 -Wall -fomit-frame-pointer
	)
endif()


if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	add_compile_options(
		-O2 -Wall -fomit-frame-pointer
	)
endif()



TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(NOT IS_BIG_ENDIAN)
	add_definitions(-DGSPLUS_LITTLE_ENDIAN)
endif()


pkg_check_modules(SDL2 sdl2)
pkg_check_modules(FREETYPE2 freetype2)

#
# run ccmake, cmake -LH, or cmake -D...
#
set(DRIVER "SDL" CACHE STRING "Driver (SDL, X11, WIN32, FB, or HEADLESS")
option(WITH_DEBUGGER "Enable the debugger" ON)
option(WITH_HOST_FST "Enable host fst support" ON)
option(TOGGLE_STATUS "Enable F10 Toggle Status support (win32/x11)" OFF)
option(WITH_RAWNET "Enable Uthernet emulation" OFF)
option(WITH_ATBRIDGE "Enable AT Bridge" OFF)
set(READLINE "AUTO" CACHE STRING "Readline library (AUTO, NONE, READLINE, LIBEDIT)")

set(generated_headers 8inst_c.h 16inst_c.h 8inst_s.h 16inst_s.h size_c.h size_s.h 8size_s.h 16size_s.h)
add_custom_command(
	OUTPUT 8inst_c.h 16inst_c.h 8inst_s.h ${CMAKE_CURRENT_BINARY_DIR}/16inst_s.h
	COMMAND perl make_inst c 8 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/8inst_c.h
 	COMMAND perl make_inst c 16 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/16inst_c.h
 	COMMAND perl make_inst s 8 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/8inst_s.h
 	COMMAND perl make_inst s 16 instable.h > ${CMAKE_CURRENT_BINARY_DIR}/16inst_s.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/instable.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)


add_custom_command(
	OUTPUT size_c.h size_s.h 8size_s.h 16size_s.h
	COMMAND perl make_size c size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/size_c.h
	COMMAND perl make_size s size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/size_s.h
	COMMAND perl make_size 8 size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/8size_s.h
	COMMAND perl make_size 16 size_tab.h > ${CMAKE_CURRENT_BINARY_DIR}/16size_s.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/size_tab.h 
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
	OUTPUT debug_shell.c
	COMMAND re2c -W -o ${CMAKE_CURRENT_BINARY_DIR}/debug_shell.c "${CMAKE_CURRENT_SOURCE_DIR}/debug_shell.re2c"
	MAIN_DEPENDENCY debug_shell.re2c
)

add_custom_command(
	OUTPUT debug_template.c
	COMMAND re2c -W -o ${CMAKE_CURRENT_BINARY_DIR}/debug_template.c "${CMAKE_CURRENT_SOURCE_DIR}/debug_template.re2c"
	MAIN_DEPENDENCY debug_template.re2c
)

add_executable(to_pro to_pro.c)
add_executable(partls partls.c)

add_subdirectory(atbridge)
add_subdirectory(tfe)
add_subdirectory(rawnet)

if (DRIVER MATCHES "SDL")
	set(driver_code sdl2_driver.c sdl2snd_driver.c)
elseif(DRIVER MATCHES "X11")
	set(driver_code xdriver.c)
elseif(DRIVER MATCHES "FB")
	set(driver_code fbdriver.c)
elseif(DRIVER MATCHES "WIN32")
	set(driver_code scc_windriver.c win32snd_driver.c win_console.c win_generic.c)
elseif(DRIVER MATCHES "HEADLESS")
	set(driver_code headless_driver.c)
else()
	message(FATAL_ERROR "Invalid driver ${DRIVER}")
endif()

if (WIN32)
	set(host_fst_code host_common.c host_mli.c win32_host_fst.c win32_host_common.c)
else()
	set(host_fst_code host_common.c host_mli.c host_fst.c unix_host_common.c)
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/MACOSX_BUNDLE_INFO_PLIST.html
set(MACOSX_BUNDLE_INFO_STRING "${PROJECT_VERSION}, Copyright 2018 Dagen Brock")
set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2018 Dagen Brock")
set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_ICON_FILE gsp_icon.icns)
set(MACOSX_BUNDLE_GUI_IDENTIFIER com.dagenbrock.gsplus)

# https://cmake.org/Wiki/CMake:Bundles_And_Frameworks
# OS X properties.
add_executable(GSplus WIN32 MACOSX_BUNDLE
	adb.c clock.c config.c engine_c.c scc.c iwm.c 
	joystick_driver.c moremem.c paddles.c parallel.c printer.cpp sim65816.c
	smartport.c sound.c sound_driver.c video.c scc_socket_driver.c glog.c
	imagewriter.cpp scc_imagewriter.c scc_llap.c options.c

	disasm.c
	debug_shell.c
	debug_template.c

	$<$<BOOL:${WITH_DEBUGGER}>:debug.c>
	$<$<BOOL:${WITH_HOST_FST}>:${host_fst_code}>
	${driver_code}
	${generated_headers}

	$<$<BOOL:${WIN32}>:win32.rc>
	$<$<BOOL:${APPLE}>:assets/gsp_icon.icns>
	$<$<BOOL:${APPLE}>:assets/GSBug.Templates>
	$<$<BOOL:${APPLE}>:fix_mac_menu.m>
)


SET_SOURCE_FILES_PROPERTIES(
  assets/gsp_icon.icns
  PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources
)

SET_SOURCE_FILES_PROPERTIES(
  assets/GSBug.Templates
  PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources
)


if(APPLE)
	add_custom_command(TARGET GSplus POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_BINARY_DIR}/rawnet/vmnet_helper"
		"${CMAKE_CURRENT_BINARY_DIR}/GSplus.app/Contents/MacOS/vmnet_helper"
		)
	add_dependencies(GSplus vmnet_helper)
endif()
# SET_SOURCE_FILES_PROPERTIES(vmnet_helper PROPERTIES MACOSX_PACKAGE_LOCATION MacOS)

add_library(x_readline readline.c)
if (READLINE MATCHES "AUTO")
	if (CMAKE_SYSTEM_NAME MATCHES "(Darwin|FreeBSD|OpenBSD|NetBSD)")
		set(READLINE "LIBEDIT")
	endif()
	if (CMAKE_SYSTEM_NAME MATCHES "Linux")
		set(READLINE "READLINE")
	endif()
endif()
if(READLINE MATCHES "READLINE")
	target_compile_definitions(x_readline PRIVATE USE_READLINE)
	target_link_libraries(x_readline PUBLIC readline)
elseif(READLINE MATCHES "LIBEDIT")
	target_compile_definitions(x_readline PRIVATE USE_LIBEDIT)
	target_link_libraries(x_readline PUBLIC edit)
endif()

target_link_libraries(GSplus x_readline)

if (WITH_RAWNET)
	target_link_libraries(GSplus rawnet)
endif()

if (WITH_ATBRIDGE)
	target_link_libraries(GSplus atbridge)
endif()

if (DRIVER MATCHES "WIN32")
	target_link_libraries(GSplus comdlg32 Shlwapi IPHlpApi
		winmm gdi32 dsound comctl32 ws2_32 shell32
	)
	target_compile_definitions(GSplus PRIVATE WIN_SOUND)
endif()

if (DRIVER MATCHES "SDL")
	target_link_libraries(GSplus ${SDL2_LDFLAGS} ${FREETYPE2_LDFLAGS} SDL2_image)
	target_compile_options(GSplus PUBLIC ${SDL2_CFLAGS} ${FREETYPE2_CFLAGS}  -DHAVE_SDL)
endif()

if (APPLE)
	target_link_libraries(GSplus "-framework Cocoa")
endif()

if (TOGGLE_STATUS)
	target_compile_definitions(GSplus PUBLIC TOGGLE_STATUS)
endif()

if (WITH_DEBUGGER)
	target_compile_definitions(GSplus PRIVATE GSPLUS_DEBUGGER)
endif()



#if (APPLE AND DRIVER MATCHES "SDL")
#	target_compile_options(GSplus PRIVATE -F${CMAKE_CURRENT_SOURCE_DIR} )
#	target_link_libraries(GSplus -F${CMAKE_CURRENT_SOURCE_DIR} "-framework SDL2" -Wl,-rpath,@executable_path/../Frameworks)
#endif()


if (APPLE)
	add_custom_target(bundle
		DEPENDS GSplus
		COMMAND dylibbundler -od -b -x GSplus.app/Contents/MacOS/GSplus -d GSplus.app/Contents/libs
		COMMENT bundling libraries...
	)
	add_custom_target(setuid
		COMMAND sudo chown root GSplus.app/Contents/MacOS/vmnet_helper
		COMMAND sudo chmod +s GSplus.app/Contents/MacOS/vmnet_helper
		USES_TERMINAL)
	add_dependencies(setuid vmnet_helper)
endif()
