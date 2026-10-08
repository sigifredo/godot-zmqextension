# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Qué es

Extensión GDExtension para Godot 4.5 (C++23, godot-cpp como submódulo en la rama 4.5). Expone el nodo `ZmqSubscriber`, que recibe mensajes de un socket ZeroMQ SUB (libzmq, C-API) y los entrega a GDScript por señales. El caso de uso del demo es transmitir video como JPEG por ZMQ PUB/SUB.

## Comandos

```bash
git submodule update --init          # godot-cpp (necesario la primera vez)
cmake -S . -B build/debug            # Debug por defecto -> godot-cpp template_debug
cmake --build build/debug -j4   # no usar -j sin número: agota la RAM
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release   # template_release
```

- Requiere `libzmq` visible por `pkg-config` (`PkgConfig::ZMQ`).
- La biblioteca se escribe directamente en `demo/bin/` como `libzmqstream.<plataforma>.<template>.<arch>.<so|dylib>` (Linux `.so`, macOS `.dylib`; en macOS instalar con `brew install zeromq pkg-config`). Ese nombre debe coincidir con `demo/bin/zmq_stream.gdextension`; `GODOTCPP_SUFFIX` lo define godot-cpp. Los `.so` y `.dylib` están en `.gitignore`.
- `GODOTCPP_TARGET` se deriva del tipo de build; se puede forzar con `-DGODOTCPP_TARGET=editor`.
- No hay tests ni linter configurados.
- Probar el demo: abrir `demo/` en Godot 4.5 y ejecutar en paralelo el publicador de prueba (el endpoint por defecto de ambos lados es `tcp://127.0.0.1:5555`; el suscriptor hace `connect`, el publicador hace `bind`):

```bash
~/.python3-venv/bin/python tools/PublishJpeg.py --path tools/samples --fps 30   # --camera requiere opencv-python-headless
```

## Arquitectura

- `src/RegisterTypes.cpp`: punto de entrada `ZmqStreamInit` (debe coincidir con `entry_symbol` del `.gdextension`); registra las clases en el nivel `SCENE`. Una clase nueva se registra aquí.
- `src/stream/ZmqSubscriber.cpp`: modelo de hilos que condiciona cualquier cambio:
  - El contexto ZMQ se crea en `start()` y se destruye en `stop()`. El socket se crea, usa y cierra únicamente dentro de `runThread` (hilo propio). El hilo recibe copias de la configuración; nunca lee propiedades del nodo.
  - El hilo de red solo escribe en `messages_` y `errors_` bajo `mutex_`. Los tipos de Godot (`String`, `PackedByteArray`) y `emit_signal` se usan solo en `_process`, en el hilo principal (máximo `MaxMessagesPerFrame` por frame).
  - Backpressure: `enqueueMessage` descarta el mensaje más antiguo al superar `maxQueue` (cuenta en `dropped_`, expuesto como `get_dropped_count()`). `RCVHWM` se fija en `maxQueue * 2` porque un mensaje puede ocupar dos frames.
  - Mensajes multi-frame: el primer frame es el topic y el último el payload; con un solo frame el topic llega vacío.
  - `stop()` es acotado por `PollTimeoutMs` (100 ms) del `zmq_poll`; `LINGER=0`. Si el hilo muere por error fatal, `start()` llama a `stop()` para recuperar antes de relanzar.
  - La configuración (`endpoint`, `topic`, `bind_socket`, `max_queue`) se captura en `start()`; cambiarla con el nodo activo requiere `stop()` y `start()`.
- `demo/`: proyecto Godot. `Main.gd` conecta `message_received` a `VideoView.push_jpeg`, que decodifica el JPEG y reutiliza el `ImageTexture` si el tamaño no cambia.
- `tmp/` está en `.gitignore` (borradores, p. ej. `titeres.cpp`); no es parte del build.

## Convenciones (del proyecto)

- Cabeceras en `include/` con extensión `.hpp`, guardas `#ifndef`/`#define`/`#endif`, Doxygen, strings con comillas dobles. Los includes se agrupan con comentarios (`// own`, `// godot`, `// std`, `// zmq`) y se resuelven desde `include/` (p. ej. `<stream/ZmqSubscriber.hpp>`).
- Miembros y métodos en orden alfabético por ámbito de acceso (constructor y destructor primero en `public`); el orden en el `.cpp` replica el de la declaración en el `.hpp`. Los miembros privados terminan en `_`. Los métodos bindeados a Godot usan snake_case en `_bind_methods`; el C++ usa camelCase.
- Al añadir archivos, agregarlos a `HEADERS`/`SOURCES` en `CMakeLists.txt` (no hay glob).
- Los scripts de `demo/` y `tools/` usan comillas simples y tabs en GDScript.
