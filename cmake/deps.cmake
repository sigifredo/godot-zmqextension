add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)
