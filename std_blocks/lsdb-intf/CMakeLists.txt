
install(FILES lsdb-intf.lua DESTINATION ${UBX_LUABLOCK_DIR})
install(PROGRAMS ubx-dbus DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx/lsdb-intf.d)
install(FILES ${CMAKE_SOURCE_DIR}/tests/lsdb_intf_test_plugin.lua
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx/lsdb-intf.d)

if(INSTALL_EXAMPLES)
  install(FILES test-lsdb-intf.usc DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx/examples/usc/)
endif()
