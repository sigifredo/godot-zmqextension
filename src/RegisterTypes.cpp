// own
#include <RegisterTypes.hpp>
#include <stream/ZmqSubscriber.hpp>

// godot
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initializeZmqStreamModule(ModuleInitializationLevel level)
{
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;

    ClassDB::register_class<ZmqSubscriber>();
}

void uninitializeZmqStreamModule(ModuleInitializationLevel level)
{
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

extern "C"
{
    GDExtensionBool GDE_EXPORT ZmqStreamInit(GDExtensionInterfaceGetProcAddress getProcAddress, const GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization)
    {
        GDExtensionBinding::InitObject initObject(getProcAddress, library, initialization);

        initObject.register_initializer(initializeZmqStreamModule);
        initObject.register_terminator(uninitializeZmqStreamModule);
        initObject.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return initObject.init();
    }
}
