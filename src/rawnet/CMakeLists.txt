
if(WIN32)
	set(rawnetarch rawnetarch_win32.c)
elseif(APPLE)
	set(rawnetarch rawnetarch_vmnet_helper.c)
	#set(rawnetarch rawnetarch_unix.c)
elseif(UNIX)
	set(rawnetarch rawnetarch_unix.c)
endif()

add_library(rawnet cs8900.c rawnet.c rawnetsupp.c rawnetarch.c ${rawnetarch})

target_compile_definitions(rawnet PUBLIC HAVE_RAWNET)
target_compile_definitions(rawnet PRIVATE CS8900_DEBUG RAWNET_DEBUG_FRAMES)

target_compile_options(rawnet PRIVATE -g)

if(WIN32)
	target_link_libraries(rawnet ws2_32) # winsock2
elseif(APPLE)
	#target_link_libraries(rawnet PRIVATE pcap)
	#target_link_libraries(rawnet PRIVATE "-framework vmnet")
	add_executable(vmnet_helper vmnet_helper.c)
	target_link_libraries(vmnet_helper PRIVATE "-framework vmnet")

elseif(UNIX)
	target_link_libraries(rawnet PRIVATE pcap)
endif()