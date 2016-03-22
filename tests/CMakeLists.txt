
include_directories("${CMAKE_SOURCE_DIR}/service")
include_directories(${GIO_INCLUDE_DIRS})

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

##############
# Formatting
##############

add_custom_target(format
	COMMAND clang-format -i -style=file
	agent-test.cpp
	policykit-mock.h
	notifications-mock.h
)
