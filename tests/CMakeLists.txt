
include_directories("${CMAKE_SOURCE_DIR}/service")
include_directories(${GIO_INCLUDE_DIRS})
include_directories(${NOTIFY_INCLUDE_DIRS})

##############
# Google Test
##############

set (GTEST_SOURCE_DIR /usr/src/gtest/src)
set (GTEST_INCLUDE_DIR ${GTEST_SOURCE_DIR}/..)

add_subdirectory("/usr/src/gmock" gmock)

include_directories(${GTEST_INCLUDE_DIR})

##############
# DBus Test
##############

pkg_check_modules(DBUSTEST REQUIRED dbustest-1)
include_directories(${DBUSTEST_INCLUDE_DIRS})

##############
# PK Agent
##############

add_executable (agent-test
	agent-test.cpp
)

target_link_libraries(agent-test
	gmock_main
	service-lib
	${DBUSTEST_LIBRARIES}
)

add_test (NAME agent-test
	COMMAND agent-test
)

set_property(GLOBAL APPEND PROPERTY FORMAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/agent-test.cpp")

##############
# AuthManager
##############

add_executable (auth-manager-test
	auth-manager-test.cpp
)

target_link_libraries(auth-manager-test
	gmock_main
	service-lib
	${DBUSTEST_LIBRARIES}
)

add_test (NAME auth-manager-test
	COMMAND auth-manager-test
)

set_property(GLOBAL APPEND PROPERTY FORMAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/auth-manager-test.cpp")

##############
# Authentication
##############

add_executable (authentication-test
	authentication-test.cpp
)

target_link_libraries(authentication-test
	gmock_main
	service-lib
	${DBUSTEST_LIBRARIES}
)

add_test (NAME authentication-test
	COMMAND authentication-test
)

set_property(GLOBAL APPEND PROPERTY FORMAT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/authentication-test.cpp")

