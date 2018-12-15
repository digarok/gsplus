
if(WIN32)
	set(rawnetarch rawnetarch_win32.c)
elseif(APPLE)
	set(rawnetarch rawnetarch_darwin.c)
elseif(UNIX)
	set(rawnetarch rawnetarch_unix.c)
endif()

add_library(rawnet cs8900.c rawnet.c rawnetsupp.c ${rawnetarch})

target_compile_definitions(rawnet PUBLIC HAVE_RAWNET)
target_compile_definitions(rawnet PRIVATE CS8900_DEBUG CS8900_DEBUG_STORE CS8900_DEBUG_LOAD)

if(WIN32)
elseif(APPLE)
	target_link_libraries(rawnet PRIVATE "-framework vmnet")
elseif(UNIX)
	target_link_libraries(rawnet PRIVATE pcap)
endif()