# liblfb: lock-free broadcast buffer (header-only)

add_library(lfb INTERFACE)
target_include_directories(lfb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

install(FILES lfb.h lfb_shm.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ubx/ COMPONENT dev)

# stress test
add_executable(test-lfb test-lfb.c)
target_link_libraries(test-lfb PRIVATE lfb rt pthread)
install(TARGETS test-lfb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# multi-process (forked readers) latency/throughput benchmark
add_executable(bench-lfb-shm bench-lfb-shm.c)
target_link_libraries(bench-lfb-shm PRIVATE lfb rt)
install(TARGETS bench-lfb-shm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# note: -Wno-null-dereference because the unit tests dereference
# lfb_user()/lfb_shm_handle() results that cmocka asserts established as
# non-NULL, which gcc's analysis cannot follow
if(NOT CMOCKA_FOUND)
  message(WARNING "cmocka not found, not building lfb unit tests")
else(NOT CMOCKA_FOUND)
  add_executable(test-lfb-unit test-lfb-unit.c)
  target_compile_options(test-lfb-unit PRIVATE
    -Wall -Wextra -Werror -Wno-null-dereference)
  target_link_libraries(test-lfb-unit lfb ${CMOCKA_LIBRARIES})
  install(TARGETS test-lfb-unit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

  add_executable(test-lfb-shm-unit test-lfb-shm-unit.c)
  target_compile_options(test-lfb-shm-unit PRIVATE
    -Wall -Wextra -Werror -Wno-null-dereference)
  target_link_libraries(test-lfb-shm-unit lfb ${CMOCKA_LIBRARIES} rt)
  install(TARGETS test-lfb-shm-unit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif(NOT CMOCKA_FOUND)
