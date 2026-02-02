add_library(nsbmp ${WISP_LIB_TYPE}
	src/libnsbmp.c
)
set_target_properties(nsbmp PROPERTIES SOVERSION ${WISP_ABI})

if(NOT WIN32)
install(TARGETS nsbmp
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
endif()