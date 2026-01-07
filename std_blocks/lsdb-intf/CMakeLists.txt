
install(FILES lsdb-intf.lua DESTINATION ${UBX_LUABLOCK_DIR})
install(PROGRAMS ubx-dbus DESTINATION ${CMAKE_INSTALL_BINDIR})

if(INSTALL_EXAMPLES)
  install(FILES test-lsdb-intf.usc DESTINATION ${CMAKE_INSTALL_DATADIR}/ubx/examples/usc/)
endif()
