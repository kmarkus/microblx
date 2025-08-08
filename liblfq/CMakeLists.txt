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
install(TARGETS test-lfq RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NOT CMOCKA_FOUND)
  message(WARNING "cmocka not found, not building lfq unit tests")
else(NOT CMOCKA_FOUND)
  add_executable(test-lfq-unit test-lfq-unit.c)
  target_compile_options(test-lfq-unit PRIVATE -Wall -Wextra -Werror)
  target_link_libraries(test-lfq-unit lfq ${CMOCKA_LIBRARIES})
  install(TARGETS test-lfq-unit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif(NOT CMOCKA_FOUND)
