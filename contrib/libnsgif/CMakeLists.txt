add_library(nsgif ${WISP_LIB_TYPE}
	src/gif.c
	src/lzw.c
)
set_target_properties(nsgif PROPERTIES SOVERSION ${WISP_ABI})

if(NOT WIN32)
install(TARGETS nsgif
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
endif()