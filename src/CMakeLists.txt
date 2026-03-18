#########################################################################################
#
# CMakeLists.txt for ODSAS
#
#########################################################################################
# Set CMake defaults
cmake_minimum_required(VERSION 2.8.12)
project(ODSAS)

#########################################################################################
# To run this script, you need to have CMake installed and the GDAL library available.  
# Make sure to adjust the paths to your GDAL installation and vcpkg as needed.
# This script sets up a CMake project for the ODSAS (Open Digital Shoreline System Analysis) tool,
# which is a C++ application that uses the GDAL library for geospatial data processing. 
# In a typical workflow, you would run the following commands in your terminal:
# From the main project directory (assuming CMakeLists.txt is located in src/):
# cmake -S src -B build
# cmake -B build -S src -DCMAKE_TOOLCHAIN_FILE=C:/Users/agarcia/Dev/vcpkg/scripts/buildsystems/vcpkg.cmake 
# cmake --build build --config Release
# move to build directory and run the executable:
# cd build
# cmake --install ..\build
# To run the executable:
# .\odsas.exe
# Clean previous build files
#

# System-specific configuration
if(UNIX AND NOT APPLE)
   message(STATUS "Configuring for UNIX/Linux")
   # Add UNIX/Linux specific settings here
endif()

if(APPLE)
   message(STATUS "Configuring for macOS")
   # Add macOS specific settings here
endif()

if(WIN32 AND NOT CYGWIN AND NOT MINGW)
   message(STATUS "Configuring for Windows (MSVC)")
   # Windows/MSVC specific settings
   if(MSVC)
      set(VCPKG_INSTALLED_DIR "C:\\vcpkg\\installed\\x64-windows")
      set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${VCPKG_INSTALLED_DIR}")

      # Always set GDAL include and library paths to vcpkg installation
      set(GDAL_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include")
      set(GDAL_LIBRARY "${VCPKG_INSTALLED_DIR}/lib/gdal.lib")
      set(GDAL_BIN_DIR "${VCPKG_INSTALLED_DIR}/bin")

      set(GDAL_INCLUDE_DIRS ${GDAL_INCLUDE_DIR})
      set(GDAL_LIBRARIES ${GDAL_LIBRARY})

      # Set Boost include dir explicitly for MSVC/vcpkg
      set(BOOST_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include/boost")

      # Set PROJ_LIB to the path where proj.db is located
      set(PROJ_LIB "${VCPKG_INSTALLED_DIR}/share/proj")
      find_package(PROJ REQUIRED)
      add_definitions(-DPROJ_LIB="${PROJ_LIB}")
      set(PROJ_LIB "${PROJ_LIB}" CACHE PATH "Path to PROJ library" FORCE)
      message(STATUS "PROJ_LIB is set to: ${PROJ_LIB}")

      find_package(unofficial-sqlite3 CONFIG REQUIRED)
   endif()
endif()

if(CYGWIN)
   message(STATUS "Configuring for Cygwin")
   # Add Cygwin specific settings here
endif()

if(MINGW)
   message(STATUS "Configuring for MinGW")
   # Add MinGW specific settings here
endif()

# Set up some c++ and architecture defaults
enable_language(CXX)
set(FIND_LIBRARY_USE_LIB64_PATHS true)
cmake_minimum_required(VERSION 3.15)

# Set policy CMP0177 to NEW to normalize install() DESTINATION paths
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()


# Set up project-specific defaults
file(GLOB ODSAS_SOURCE_FILES *.cpp)
set(ODSAS_EXECUTABLE odsas)
set(CMAKE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")      # Assume that CMakeLists.txt is in the src folder
set(ODSAS_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug)                                 # If not specified, assume a Debug build
endif(NOT CMAKE_BUILD_TYPE)


# Set output directories for each build type
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${ODSAS_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ODSAS_BINARY_DIR}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${ODSAS_BINARY_DIR}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${ODSAS_BINARY_DIR}/MinSizeRel)

# Set flags for each build type
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -DCPU -Dx86 -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DCPU -Dx86")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DCPU -Dx86")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DCPU -Dx86")

# Set up compiler-independent flags and build directories for the various build types
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -DCPU -Dx86")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DCPU -Dx86")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DCPU -Dx86")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DCPU -Dx86")


#########################################################################################
# Detect the compiler and set compiler-specific flags etc. TODO this isn't finished
message("Checking compiler")

if(CMAKE_COMPILER_IS_GNUCC)
   message("C++ compiler is ${CMAKE_CXX_COMPILER_ID} g++ ${CMAKE_CXX_COMPILER_VERSION}")

   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -W -Wall -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings -Wredundant-decls")

   # For Debug builds under gcc
   if(CMAKE_BUILD_TYPE MATCHES Debug)
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.8")
         # Undefined behaviour sanitizer introduced with gcc 4.9, see http://developerblog.redhat.com/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=integer-divide-by-zero -fsanitize=unreachable -fsanitize=null -fsanitize=return -fsanitize=address")
      endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.8")

      # TODO I think these were introduced in a later gcc version, need to check
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "5.0")
         # Undefined behaviour sanitizer introduced with gcc 4.9, see http://developerblog.redhat.com/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=vla-bound -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fsanitize=bounds -fsanitize=alignment -fsanitize=object-size -fsanitize=vptr")
         # TODO not yet working
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmpx -fcheck-pointer-bounds -fsanitize-blacklist=blacklist.txt")
      endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "5.0")


#   if(CMAKE_BUILD_TYPE MATCHES Debug)
#      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")
   endif(CMAKE_BUILD_TYPE MATCHES Debug)

   # For RelWithDebInfo builds under gcc, set flag for profiling with gprof
   if(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
   endif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

   # For Release builds under gcc, add an extra flag
   if(CMAKE_BUILD_TYPE MATCHES Release)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer")
   endif(CMAKE_BUILD_TYPE MATCHES Release)

   # Finally set some gcc-specific linker flags
   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed -Wl,--no-undefined")
endif(CMAKE_COMPILER_IS_GNUCC)

if(MSVC)
   message("Compiler is Visual Studio")
   # Force the use of g++.exe (adjust the path if needed)
   if(NOT DEFINED CMAKE_CXX_COMPILER)
   set(CMAKE_C_COMPILER "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx86\\x86\\cl.exe" CACHE FILEPATH "C compiler" FORCE)
   set(CMAKE_CXX_COMPILER "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx86\\x86\\cl.exe" CACHE FILEPATH "C++ compiler" FORCE)
   endif()
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")

   set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${VCPKG_INSTALLED_DIR}")

   # Always set GDAL include and library paths to vcpkg installation
   set(GDAL_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include")
   set(GDAL_LIBRARY "${VCPKG_INSTALLED_DIR}/lib/gdal.lib")
   set(GDAL_BIN_DIR "${VCPKG_INSTALLED_DIR}/bin")

   set(GDAL_INCLUDE_DIRS ${GDAL_INCLUDE_DIR})
   set(GDAL_LIBRARIES ${GDAL_LIBRARY})

   # Set PROJ_LIB to the path where proj.db is located
   set(PROJ_LIB "${VCPKG_INSTALLED_DIR}/share/proj")
   find_package(PROJ REQUIRED)
   # Pass it to the build environment
   add_definitions(-DPROJ_LIB="${PROJ_LIB}")

   # Set the PROJ_LIB environment variable for the build process
   set(PROJ_LIB "${PROJ_LIB}" CACHE PATH "Path to PROJ library" FORCE)
   # Optional: Print the Path for Debugging
   message(STATUS "PROJ_LIB is set to: ${PROJ_LIB}")

   #set("unofficial-sqlite3_DIR" "C:/Users/agarcia/Dev/vcpkg/installed/x64-windows/share/unofficial-sqlite3")
   find_package(unofficial-sqlite3 CONFIG REQUIRED)
   
   # To handle large files with many templates (like Eigen), increase the maximum number of sections in an object file
   add_compile_options(/bigobj)

endif(MSVC)

# Eigen requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#########################################################################################
# Find libraries
message("Finding libraries")

# Library EIGEN
#find_package(Eigen3 3.3 REQUIRED NO_MODULE)
#message(STATUS "Eigen3 version: ${EIGEN3_VERSION_STRING}")
#message(STATUS "Eigen3 include dir: ${EIGEN3_INCLUDE_DIR}")


find_path(EIGEN3_INCLUDE_DIR
  NAMES Eigen/Dense
  PATHS
    ${CMAKE_PREFIX_PATH}
    $ENV{CMAKE_PREFIX_PATH}
    $ENV{CONDA_PREFIX}/include/eigen3
  PATH_SUFFIXES eigen3
)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

if(NOT EIGEN3_INCLUDE_DIR)
  message(FATAL_ERROR "Could not find Eigen3 headers")
else()
  include_directories(${EIGEN3_INCLUDE_DIR})
  message(STATUS "Found Eigen3 in ${EIGEN3_INCLUDE_DIR}")
endif()




# The model cannot be built without GDAL

find_path(GDAL_INCLUDE_DIR gdal.h
  PATHS ENV GDAL_INCLUDE_DIR
  PATH_SUFFIXES gdal
)

find_library(GDAL_LIBRARY gdal
  PATHS ENV GDAL_LIBRARY
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDAL DEFAULT_MSG GDAL_LIBRARY GDAL_INCLUDE_DIR)

find_package(GDAL REQUIRED)
message(STATUS "GDAL_LIBRARIES=${GDAL_LIBRARIES}")
message(STATUS "GDAL_INCLUDE_DIRS=${GDAL_INCLUDE_DIRS}")

# Configure GDAL_DATA environment variable for cross-platform compatibility
if(WIN32 AND NOT CYGWIN AND NOT MINGW)
    # Windows with vcpkg
    if(MSVC)
        set(GDAL_DATA_DIR "${VCPKG_INSTALLED_DIR}/share/gdal")
        if(EXISTS "${GDAL_DATA_DIR}")
            # Convert backslashes to forward slashes for C++ string literals
            string(REPLACE "\\" "/" GDAL_DATA_DIR_NORMALIZED "${GDAL_DATA_DIR}")
            add_definitions(-DGDAL_DATA="${GDAL_DATA_DIR_NORMALIZED}")
            message(STATUS "GDAL_DATA set to: ${GDAL_DATA_DIR_NORMALIZED}")
        else()
            message(WARNING "GDAL_DATA directory not found at: ${GDAL_DATA_DIR}")
        endif()
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux - typically with conda
    if(DEFINED ENV{CONDA_PREFIX})
        set(GDAL_DATA_DIR "$ENV{CONDA_PREFIX}/share/gdal")
        if(EXISTS "${GDAL_DATA_DIR}")
            add_definitions(-DGDAL_DATA="${GDAL_DATA_DIR}")
            message(STATUS "GDAL_DATA set to: ${GDAL_DATA_DIR}")
        else()
            message(WARNING "GDAL_DATA directory not found at: ${GDAL_DATA_DIR}")
        endif()
    else()
        # Fallback to system GDAL installation
        find_program(GDAL_CONFIG gdal-config)
        if(GDAL_CONFIG)
            execute_process(COMMAND ${GDAL_CONFIG} --datadir
                OUTPUT_VARIABLE GDAL_DATA_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(EXISTS "${GDAL_DATA_DIR}")
                add_definitions(-DGDAL_DATA="${GDAL_DATA_DIR}")
                message(STATUS "GDAL_DATA set to: ${GDAL_DATA_DIR}")
            endif()
        endif()
    endif()
elseif(APPLE)
    # macOS - could be conda or system GDAL
    if(DEFINED ENV{CONDA_PREFIX})
        set(GDAL_DATA_DIR "$ENV{CONDA_PREFIX}/share/gdal")
        if(EXISTS "${GDAL_DATA_DIR}")
            add_definitions(-DGDAL_DATA="${GDAL_DATA_DIR}")
            message(STATUS "GDAL_DATA set to: ${GDAL_DATA_DIR}")
        endif()
    else()
        # Try to find via gdal-config
        find_program(GDAL_CONFIG gdal-config)
        if(GDAL_CONFIG)
            execute_process(COMMAND ${GDAL_CONFIG} --datadir
                OUTPUT_VARIABLE GDAL_DATA_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(EXISTS "${GDAL_DATA_DIR}")
                add_definitions(-DGDAL_DATA="${GDAL_DATA_DIR}")
                message(STATUS "GDAL_DATA set to: ${GDAL_DATA_DIR}")
            endif()
        endif()
    endif()
endif()

set(LIBS ${LIBS} ${GDAL_LIBRARIES} ${Eigen3_LIBRARIES})
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${GDAL_INCLUDE_DIRS} ${Eigen3_INCLUDE_DIRS})
message(STATUS "LIBS=${LIBS}")
message(STATUS "CMAKE_INCLUDE_PATH=${CMAKE_INCLUDE_PATH}")

# Added by DFM, stolen from https://github.com/qgis/QGIS/blob/master/cmake/FindGDAL.cmake
set(GDAL_CONFIG_PREFER_PATH "$ENV{GDAL_HOME}/bin" CACHE STRING "preferred path to GDAL (gdal-config)")
set(GDAL_CONFIG_PREFER_FWTOOLS_PATH "$ENV{FWTOOLS_HOME}/bin_safe" CACHE STRING "preferred path to GDAL (gdal-config) from FWTools")
find_program(GDAL_CONFIG gdal-config
   ${GDAL_CONFIG_PREFER_PATH}
   ${GDAL_CONFIG_PREFER_FWTOOLS_PATH}
   /usr/local/bin/
   /usr/bin/
)

message("DBG GDAL_CONFIG ${GDAL_CONFIG}")

# Dynamically linking the shared libaries 
#message( "Found environment variable" LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH} )


#########################################################################################
# Finally, the important bits
message("Writing Makefile")
## Modern CMake: use only target_include_directories for your executable
add_executable(${ODSAS_EXECUTABLE} ${ODSAS_SOURCE_FILES})
find_package(Boost REQUIRED)
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Eigen3 include dir: ${EIGEN3_INCLUDE_DIR}")
if(NOT Boost_INCLUDE_DIRS)
   message(FATAL_ERROR "Boost_INCLUDE_DIRS is empty! Boost not found or not installed in vcpkg?")
endif()
if(NOT EIGEN3_INCLUDE_DIR)
   message(FATAL_ERROR "EIGEN3_INCLUDE_DIR is empty! Eigen3 not found or not installed in vcpkg?")
endif()
target_include_directories(${ODSAS_EXECUTABLE} PRIVATE
   ${GDAL_INCLUDE_DIRS}
   ${EIGEN3_INCLUDE_DIR}
   ${Boost_INCLUDE_DIRS}
   "C:/Users/agarcia/Dev/vcpkg/installed/x64-windows/include"
)
target_link_libraries(${ODSAS_EXECUTABLE} ${LIBS} Eigen3::Eigen)
install(TARGETS ${ODSAS_EXECUTABLE} RUNTIME DESTINATION ${ODSAS_INSTALL_DIR})


# Tell the user what has happened
message(STATUS "CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_INCLUDE_PATH=${CMAKE_INCLUDE_PATH}")
message(STATUS "LIBS=${LIBS}")
message(STATUS "ODSAS_INSTALL_DIR=${ODSAS_INSTALL_DIR}")
message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")


#########################################################################################
# Copy the executable to the main folder after build
add_custom_command(TARGET ${ODSAS_EXECUTABLE} POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_if_different
   $<TARGET_FILE:${ODSAS_EXECUTABLE}>
   "${CMAKE_SOURCE_DIR}/../$<TARGET_FILE_NAME:${ODSAS_EXECUTABLE}>"
)

# Copy odsas.ini to the project root after build (for both Debug and Release)
add_custom_command(TARGET ${ODSAS_EXECUTABLE} POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_if_different
   "${CMAKE_SOURCE_DIR}/../ODSAS.ini"
   "${CMAKE_SOURCE_DIR}/../ODSAS.ini"
)