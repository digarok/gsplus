
if (WIN32)
	set(rawnetarch rawnetarch_win32.c)
elseif(APPLE)
	set(rawnetarch rawnetarch_darwin.c)
elseif(UNIX)
	set(rawnetarch rawnetarch_unix.c)
endif()

add_library(rawnet cs8900.c rawnet.c rawnetsupp.c ${rawnetarch})

target_compile_definitions(rawnet PUBLIC HAVE_RAWNET)

