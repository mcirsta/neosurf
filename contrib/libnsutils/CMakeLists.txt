add_library(nsutils ${WISP_LIB_TYPE}
	src/base64.c
	src/time.c
	src/unistd.c
	src/libwapcaplet.c
)
set_target_properties(nsutils PROPERTIES SOVERSION ${WISP_ABI})

if(NOT WIN32)
install(TARGETS nsutils
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
endif()