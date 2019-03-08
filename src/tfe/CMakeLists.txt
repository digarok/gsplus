set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(tfe tfe.c tfearch.c tfesupp.c)

target_compile_definitions(tfe PUBLIC HAVE_TFE)

if(WIN32)
	target_include_directories(tfe PRIVATE ../rawnet/include)
	target_link_libraries(tfe ws2_32) # winsock2
endif()