string(TIMESTAMP _time_stamp "%Y%m%d")

message(STATUS "Resolving GIT Version")
 
set(_build_version "unknown")
 
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${local_dir}"
    OUTPUT_VARIABLE _build_version
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message( STATUS "GIT hash: ${_build_version}")
else()
  message(STATUS "GIT not found")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCOMPILEDATE=\\\"${_time_stamp}\\\" -DGITREV=\\\"${_build_version}\\\"")

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "")
