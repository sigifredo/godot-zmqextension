#ifndef REGISTERTYPES_HPP
#define REGISTERTYPES_HPP

// godot
#include <godot_cpp/godot.hpp>

/**
 * Registra las clases de la extensión al alcanzar el nivel SCENE.
 *
 * @param level Nivel de inicialización que se está procesando.
 */
void initializeZmqStreamModule(godot::ModuleInitializationLevel level);

/**
 * Libera los recursos de la extensión al descargar el nivel SCENE.
 *
 * @param level Nivel de inicialización que se está procesando.
 */
void uninitializeZmqStreamModule(godot::ModuleInitializationLevel level);

#endif
