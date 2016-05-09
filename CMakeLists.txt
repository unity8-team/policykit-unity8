project(policykit-unity8 CXX)
cmake_minimum_required (VERSION 3.0.0)

set (PACKAGE_VERSION "0.1")
set (PACKAGE ${CMAKE_PROJECT_NAME})

##
## Directories
##

include (GNUInstallDirs)
if (EXISTS "/etc/debian_version") 
	set(CMAKE_INSTALL_LIBEXECDIR ${CMAKE_INSTALL_LIBDIR})
	set(CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}")
endif()

set(CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${CMAKE_PROJECT_NAME}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Werror -std=c++14 -pthread ${GCOV_FLAGS}")

##
## Pkgconfig
##

find_package(PkgConfig REQUIRED)
pkg_check_modules(POLICYKIT REQUIRED polkit-agent-1)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(NOTIFY REQUIRED libnotify)
# pkg_check_modules(WHOOPSIE REQUIRED libwhoopsie)

##
## Go deeper into the tree
##

add_subdirectory(service)
add_subdirectory(data)
add_subdirectory(po)

##
## Testing
##

option (enable_tests "Build the package's automatic tests." ON)

if (${enable_tests})
  enable_testing()
endif()

add_subdirectory(tests)

##
## Coverage
##

include(EnableCoverageReport)
set(filter-list)
list(APPEND filter-list "/usr")
list(APPEND filter-list "${CMAKE_SOURCE_DIR}/tests")
list(APPEND filter-list "${CMAKE_SOURCE_DIR}/service/glib-thread.*")
if (NOT ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  list(APPEND filter-list "${CMAKE_BINARY_DIR}")
endif()

ENABLE_COVERAGE_REPORT(
  TARGETS service-lib policykit-agent
  TESTS agent-test authentication-test auth-manager-test
  FILTER ${filter-list}
)
