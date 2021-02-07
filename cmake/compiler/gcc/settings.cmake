# Set build-directive (used in core to tell which buildtype we used)
add_definitions(-D_BUILD_DIRECTIVE='"${CMAKE_BUILD_TYPE}"')

set (GCC_EXPECTED_VERSION 5.0.0)

if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS GCC_EXPECTED_VERSION)
	message(FATAL_ERROR "GCC: Horizon requires version ${GCC_EXPECTED_VERSION} to build but found ${CMAKE_CXX_COMPILER_VERSION}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
message(STATUS "GCC: Enabled c++17 support")

message(STATUS "GCC: Enabled C99 support")

if (PLATFORM EQUAL 32)
	# Required on 32-bit systems to enable SSE2 (standard on x64)
	set(SSE_FLAGS "-msse2 -mfpmath=sse")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SSE_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SSE_FLAGS}")
endif()
add_definitions(-DHAVE_SSE2 -D__SSE2__)
message(STATUS "GCC: SFMT enabled, SSE2 flags forced")

if (WITH_WARNINGS)
	set(WARNING_FLAGS "-W -Wall -Wextra -Winit-self -Winvalid-pch -Wfatal-errors")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} -Woverloaded-virtual")
	message(STATUS "GCC: All warnings enabled")
endif()

if (WITH_CORE_DEBUG)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3")
	message(STATUS "GCC: Debug-flags set (-g3)")
endif()

if (WITH_COVERAGE)
	set(COVERAGE_FLAGS "--coverage -ftest-coverage")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}")
	message(STATUS "GCC: Coverage flags set (${COVERAGE_FLAGS})")
endif()

set(SANITIZERS "")
if (WITH_TSAN)
	if (WITH_ASAN OR WITH_LSAN)
		message(WARNING "GCC: Thread Sanitizer cannot be used in conjunction with Address and Leak sanitizers.")
	endif()
	set(SANITIZERS "${SANITIZERS} -fsanitize=thread")
else ()
	if (WITH_ASAN)
		set(SANITIZERS "${SANITIZERS} -fsanitize=address")
	endif()
	if (WITH_LSAN)
		set(SANITIZERS "${SANITIZERS} -fsanitize=leak")
	endif()
endif()

if (WITH_STACK_CHECK)
	set(SANITIZERS "${SANITIZERS} -fstack-check -fstack-protector-strong")
endif()

if (SANITIZERS)
	message(STATUS "GCC: Sanitization enabled (${SANITIZERS} ).")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZERS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZERS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZERS}")
endif()

