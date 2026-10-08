set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# El tipo de build elige la plantilla de godot-cpp: Debug usa template_debug
# (la que carga el editor) y Release usa template_release (exportación).
# Pasar -DGODOTCPP_TARGET=editor sigue siendo posible.
if(NOT GODOTCPP_TARGET)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GODOTCPP_TARGET template_release CACHE STRING "Plantilla de godot-cpp")
    else()
        set(GODOTCPP_TARGET template_debug CACHE STRING "Plantilla de godot-cpp")
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
