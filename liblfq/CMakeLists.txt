add_library(lfq SHARED lfq.c)

set_target_properties(lfq PROPERTIES
  VERSION "${PROJECT_VERSION}"
  PUBLIC_HEADER lfq.h
)

install(TARGETS lfq
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev)

# tests
add_executable(test-lfq test-lfq.c)
target_link_libraries(test-lfq PRIVATE lfq rt pthread)
