cmake_minimum_required(VERSION 3.10)
project(web_analysis C)

set(CMAKE_C_STANDARD 99)

# Find JNI package
find_package(JNI REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${JNI_INCLUDE_DIRS})

# Source files
set(SOURCES
        web_resource_analyzer.c
        web_parsers.c
)

# JNI source files
set(JNI_SOURCES
        com_gdme_plugins_webpulseforecast_WebPulseForecastNative.c
)

# Function to configure a target for a specific platform
function(configure_platform_target TARGET_NAME SOURCES PLATFORM_SUFFIX PLATFORM_PREFIX)
    add_library(${TARGET_NAME} SHARED ${SOURCES} ${HEADERS})
    set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME ${TARGET_NAME}
            PREFIX "${PLATFORM_PREFIX}"
            SUFFIX "${PLATFORM_SUFFIX}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${PLATFORM_SUFFIX}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${PLATFORM_SUFFIX}"
            POSITION_INDEPENDENT_CODE ON
    )
    if(${TARGET_NAME} MATCHES ".*jni.*")
        target_link_libraries(${TARGET_NAME} ${JNI_LIBRARIES})
    endif()
endfunction()

# Windows targets (when building on Windows)
if(WIN32)
    configure_platform_target("web_analysis_lib" "${SOURCES}" ".dll" "")
    configure_platform_target("web_analysis_jni" "${JNI_SOURCES};${SOURCES}" ".dll" "")
endif()

# macOS native targets
if(APPLE)
    # Native macOS .dylib targets
    configure_platform_target("web_analysis_lib_mac" "${SOURCES}" ".dylib" "lib")
    configure_platform_target("web_analysis_jni_mac" "${JNI_SOURCES};${SOURCES}" ".dylib" "lib")

    # Linux .so targets (cross-compilation on macOS)
    configure_platform_target("web_analysis_lib_linux" "${SOURCES}" ".so" "lib")
    configure_platform_target("web_analysis_jni_linux" "${JNI_SOURCES};${SOURCES}" ".so" "lib")

    # Special settings for Linux targets
    set_target_properties(web_analysis_lib_linux PROPERTIES
            COMPILE_FLAGS "-fPIC"
    )
    set_target_properties(web_analysis_jni_linux PROPERTIES
            COMPILE_FLAGS "-fPIC"
    )
endif()

# Linux native targets (when building on Linux)
if(UNIX AND NOT APPLE)
    configure_platform_target("web_analysis_lib" "${SOURCES}" ".so" "lib")
    configure_platform_target("web_analysis_jni" "${JNI_SOURCES};${SOURCES}" ".so" "lib")
endif()

# Create executable (optional)
option(BUILD_EXECUTABLE "Build the executable" ON)
if(BUILD_EXECUTABLE)
    add_executable(web_analysis_exe main.c ${SOURCES} ${HEADERS})
    set_target_properties(web_analysis_exe PROPERTIES
            OUTPUT_NAME "web_analysis"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Install targets
if(APPLE)
    # Install macOS targets
    install(TARGETS web_analysis_lib_mac web_analysis_jni_mac
            RUNTIME DESTINATION bin/macos
            LIBRARY DESTINATION lib/macos
            ARCHIVE DESTINATION lib/macos
    )
    # Install Linux targets
    install(TARGETS web_analysis_lib_linux web_analysis_jni_linux
            RUNTIME DESTINATION bin/linux
            LIBRARY DESTINATION lib/linux
            ARCHIVE DESTINATION lib/linux
    )
else()
    # Install native targets for Windows/Linux
    install(TARGETS web_analysis_lib web_analysis_jni
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
    )
endif()


# Function to configure executable for a specific platform
function(configure_platform_executable TARGET_NAME SOURCES PLATFORM_SUFFIX)
    add_executable(${TARGET_NAME} main.c ${SOURCES} ${HEADERS})
    set_target_properties(${TARGET_NAME} PROPERTIES
            OUTPUT_NAME "web_analysis${PLATFORM_SUFFIX}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${PLATFORM_SUFFIX}"
    )
endfunction()

if(WIN32)
    # Windows executable
    configure_platform_executable(web_analysis_exe "${SOURCES}" "")
elseif(APPLE)
    # macOS native executable
    configure_platform_executable(web_analysis_exe_mac "${SOURCES}" "_mac")
    # Linux executable (cross-compilation on macOS)
    configure_platform_executable(web_analysis_exe_linux "${SOURCES}" "_linux")

    # Add special compile flags for Linux executable
    set_target_properties(web_analysis_exe_linux PROPERTIES
            COMPILE_FLAGS "-fPIC"
    )
else()
    # Linux native executable
    configure_platform_executable(web_analysis_exe "${SOURCES}" "")
endif()

# Install executables
if(APPLE)
    # Install macOS executable
    install(TARGETS web_analysis_exe_mac
            RUNTIME DESTINATION bin/macos
    )
    # Install Linux executable
    install(TARGETS web_analysis_exe_linux
            RUNTIME DESTINATION bin/linux
    )
else()
    # Install native executable for Windows/Linux
    install(TARGETS web_analysis_exe
            RUNTIME DESTINATION bin
    )
endif()

# Install headers
install(FILES ${HEADERS} DESTINATION include)