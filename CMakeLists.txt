# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

find_package(yat CONFIG REQUIRED)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PCO_SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPco/lib64")
    set(PCO_SDK_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPco/bin64")
else()
    set(PCO_SDK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPco/lib")
    set(PCO_SDK_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdkPco/bin")
endif()

# --------------------------------------------------------------------------
# Collect sources and includes
# --------------------------------------------------------------------------

file(GLOB_RECURSE sources src/*.cpp)
list(FILTER sources EXCLUDE REGEX ".*/PcoCameraLin.cpp$")
list(FILTER sources EXCLUDE REGEX ".*/PcoCameraWin.cpp$")
set(includedirs
    include/
    sdkPco/
    sdkPco/include/
)

# --------------------------------------------------------------------------
# Target
# --------------------------------------------------------------------------

add_library(limapco ${sources})

target_include_directories(limapco
    PUBLIC 
        ${includedirs}
)

target_link_directories(limapco
    PRIVATE
    "${PCO_SDK_LIB_DIR}"
)

target_link_libraries(limapco 
    PUBLIC
        limacore
        yat::yat
    PRIVATE
        Advapi32
        SC2_Cam
)

limatools_set_library_soversion(limapco "VERSION")

# --------------------------------------------------------------------------
# Compile definitions
# --------------------------------------------------------------------------

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PCO_VERSION_RAW)
string(STRIP "${PCO_VERSION_RAW}" PCO_VERSION)

target_compile_definitions(limapco
    PUBLIC
        PCO_NAME=Pco
        PCO_VERSION=${PCO_VERSION}
        PCO_ENABLED
)

message(STATUS "Camera enabled: Pco ${PCO_VERSION}")

# Windows DLL export
if (WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(limapco
        PRIVATE
            LIBLIMAPCO_EXPORTS
    )
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------

install(
    TARGETS limapco 
    LIBRARY DESTINATION lib
)

install(
    DIRECTORY "${PCO_SDK_BIN_DIR}/"
    DESTINATION bin 
    FILES_MATCHING PATTERN "*.dll"
)