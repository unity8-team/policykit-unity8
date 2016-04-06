
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/unity8-policy-kit.conf.in
  ${CMAKE_CURRENT_BINARY_DIR}/unity8-policy-kit.conf
  @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/unity8-policy-kit.conf
  DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions"
)
