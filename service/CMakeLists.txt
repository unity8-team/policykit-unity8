
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

###################
# Library of Stuff
###################

set(LIBRARY_SOURCES
	auth-manager.h
	auth-manager.cpp
	polkit-iface.h
	polkit-iface.cpp
)

add_library(service-lib STATIC
	${LIBRARY_SOURCES}
)

###################
# Service
###################

set(SERVICE_SOURCES
	main.cpp
)

add_executable(policykit-agent
	${SERVICE_SOURCES}
)
target_link_libraries(policykit-agent
	service-lib
)
install(TARGETS policykit-agent
	RUNTIME
	DESTINATION "${CMAKE_INSTALL_FULL_LIBEXECDIR}/unity8-policy-kit/"
)

###################
# Format
###################

add_custom_target(format
	COMMAND clang-format -i -style=file ${LIBRARY_SOURCES} ${SERVICE_SOURCES}
)
