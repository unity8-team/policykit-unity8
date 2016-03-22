
include_directories(${POLICYKIT_INCLUDE_DIRS})
include_directories(${GIO_INCLUDE_DIRS})
include_directories(${NOTIFY_INCLUDE_DIRS})

add_definitions(-DPOLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE)

###################
# Library of Stuff
###################

set(LIBRARY_SOURCES
	agent.h
	agent.cpp
	agent-glib.h
	agent-glib.cpp
	auth-manager.h
	auth-manager.cpp
	authentication.h
	authentication.cpp
	glib-thread.h
	glib-thread.cpp
)

add_library(service-lib STATIC
	${LIBRARY_SOURCES}
)

target_link_libraries(service-lib
	${POLICYKIT_LIBRARIES}
	${GIO_LIBRARIES}
	${NOTIFY_LIBRARIES}
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
