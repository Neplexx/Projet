cmake_minimum_required(VERSION 3.20)
project(Projet)

if(MSVC)
    set(SFML_DIR "C:/Program Files/SFML_3.0.2/lib/cmake/SFML")
endif(MSVC)

    set(DEFS "PATH_IMG=\"${CMAKE_CURRENT_SOURCE_DIR}/fichiers/assets/\"")
    add_compile_definitions(${DEFS})

set(SFML_STATIC_LIBRARIES ON)
find_package(SFML 3 COMPONENTS Window Graphics System REQUIRED)
link_libraries(SFML::Graphics SFML::Window SFML::System)


add_subdirectory(fichiers)