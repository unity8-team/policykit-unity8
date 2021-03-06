
include_directories(
	SYSTEM
	${POLICYKIT_INCLUDE_DIRS}
	${GIO_INCLUDE_DIRS}
	${NOTIFY_INCLUDE_DIRS}
)

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
	session-iface.h
	session-iface.cpp
)

add_library(service-lib STATIC
	${LIBRARY_SOURCES}
)

target_link_libraries(service-lib
	${POLICYKIT_LIBRARIES}
	${GIO_LIBRARIES}
	${NOTIFY_LIBRARIES}
)

foreach(LIBSOURCE ${LIBRARY_SOURCES})
set_property(GLOBAL APPEND PROPERTY FORMAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${LIBSOURCE}")
endforeach()

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
	DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}
)

foreach(SERVICESRC ${SERVICE_SOURCES})
set_property(GLOBAL APPEND PROPERTY FORMAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SERVICESRC}")
endforeach()

