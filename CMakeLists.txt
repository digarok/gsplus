cmake_minimum_required(VERSION 3.1)
project(gsplus VERSION 0.14)


if(CYGWIN OR MSYS)
	set(WIN32 1)
	add_definitions(-DWIN32 -D_WIN32)
endif()

# msys/cygwin/mingw32 add dirent.h, etc.
# Visual C++ does not.   
if(MSVC)
	include_directories(include/msvc)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_compile_options(/wd4996)
endif()

# add pcap headers for win32. assume os/x, linux, etc, already have them.
if(WIN32)
	include_directories(include/npcap)
endif()

add_subdirectory(src bin)