cmake_minimum_required(VERSION 3.20)

project(OpenLiveBridge
    VERSION 0.1.0
    DESCRIPTION "A Windows live media bridge powered by libobs"
    LANGUAGES CXX)

option(OPENLIVEBRIDGE_WITH_LIBOBS "Build with libobs backend" OFF)

add_executable(OpenLiveBridge
    src/api_types.cpp
    src/main.cpp
    src/media_bridge_service.cpp
    src/obs/libobs_controller.cpp
    src/server/local_api_server.cpp
    src/util/logger.cpp
    src/virtual_camera/virtual_camera_registry.cpp
)

target_compile_features(OpenLiveBridge PRIVATE cxx_std_20)

target_include_directories(OpenLiveBridge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(OpenLiveBridge
    PRIVATE
        OPENLIVEBRIDGE_VERSION="${PROJECT_VERSION}"
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

if(MSVC)
    target_compile_options(OpenLiveBridge PRIVATE /utf-8)
endif()

if(WIN32)
    target_link_libraries(OpenLiveBridge PRIVATE Ws2_32 Advapi32)
else()
    message(FATAL_ERROR "OpenLiveBridge currently targets Windows only.")
endif()

if(OPENLIVEBRIDGE_WITH_LIBOBS)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(LibObs REQUIRED)
    target_link_libraries(OpenLiveBridge PRIVATE LibObs::LibObs)
    target_compile_definitions(OpenLiveBridge PRIVATE OPENLIVEBRIDGE_WITH_LIBOBS=1)
else()
    target_compile_definitions(OpenLiveBridge PRIVATE OPENLIVEBRIDGE_WITH_LIBOBS=0)
endif()

install(TARGETS OpenLiveBridge RUNTIME DESTINATION bin)
