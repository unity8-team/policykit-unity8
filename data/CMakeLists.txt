
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/policykit-unity8.conf.in
  ${CMAKE_CURRENT_BINARY_DIR}/policykit-unity8.conf
  @ONLY
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/policykit-unity8.service.in
  ${CMAKE_CURRENT_BINARY_DIR}/policykit-unity8.service
  @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/policykit-unity8.conf
  DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/upstart/sessions"
)

install(
  FILES policykit-unity8.override
  DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/upstart/systemd-session/upstart"
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/policykit-unity8.service
  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/user"
)
