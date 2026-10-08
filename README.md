# pp-extension

Extensión GDExtension para Godot 4.5 que recibe mensajes de un socket ZeroMQ SUB y los entrega a GDScript mediante señales. El demo incluido transmite video como JPEG por ZMQ PUB/SUB.

## Requisitos

- Godot 4.5
- CMake 3.16 o superior y un compilador con soporte de C++23
- `libzmq` con su archivo `pkg-config` (en Debian, `libzmq3-dev`)
- Submódulo `godot-cpp` (rama 4.5)

## Compilación

```bash
git submodule update --init
cmake -S . -B build/debug
cmake --build build/debug -j
```

Para la plantilla de exportación:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
```

Debug compila contra `template_debug` (la que carga el editor) y Release contra `template_release`. Se puede forzar otra plantilla con `-DGODOTCPP_TARGET=editor`. La biblioteca resultante se escribe en `demo/bin/` con el nombre que espera `demo/bin/zmq_stream.gdextension`. Hoy solo declara Linux x86_64.

## Nodo `ZmqSubscriber`

Hereda de `Node`. Recibe en un hilo propio y emite las señales en el hilo principal, durante `_process`.

| Propiedad     | Por defecto            | Descripción                                                          |
| ------------- | ---------------------- | -------------------------------------------------------------------- |
| `endpoint`    | `tcp://127.0.0.1:5555` | Dirección ZeroMQ                                                     |
| `topic`       | `""`                   | Prefijo de suscripción; vacío recibe todo                            |
| `bind_socket` | `false`                | `true` hace `bind`, `false` hace `connect`                           |
| `max_queue`   | `2`                    | Mensajes pendientes máximos; al excederlo se descarta el más antiguo |
| `auto_start`  | `true`                 | Inicia en `_ready`                                                   |

Métodos: `start()`, `stop()`, `is_running()`, `get_dropped_count()`.

Señales:

- `message_received(topic: String, payload: PackedByteArray)`
- `error_occurred(message: String)`

En un mensaje de varios frames, el primero es el topic y el último el payload. Un mensaje de un solo frame llega con topic vacío. La configuración se captura en `start()`: para cambiarla con el nodo activo hay que llamar a `stop()` y luego a `start()`.

```gdscript
@onready var _subscriber: ZmqSubscriber = $ZmqSubscriber

func _ready() -> void:
	_subscriber.message_received.connect(_on_message_received)

func _on_message_received(topic: String, payload: PackedByteArray) -> void:
	pass
```

## Demo

1. Compilar la extensión.
2. Abrir `demo/` en Godot 4.5 y ejecutar la escena `Main`.
3. Publicar JPEG en `tcp://127.0.0.1:5555`. Cualquier publicador PUB que haga `bind` en ese endpoint sirve. Con el script de prueba local `tools/PublishJpeg.py` (requiere `pyzmq`):

```bash
python tools/PublishJpeg.py --path tools/samples --fps 30
python tools/PublishJpeg.py --camera --fps 15        # requiere opencv-python-headless
```

`PublishJpeg.py` admite además `--topic`, `--stamp` (hora de envío como topic, para medir latencia) y `--corrupt-every N` (inyecta payloads inválidos).

`Main.gd` conecta `message_received` con `VideoView.push_jpeg`, que decodifica el JPEG y reutiliza el `ImageTexture` mientras el tamaño no cambie.

## Estructura

```
include/            cabeceras (.hpp)
src/                implementación; RegisterTypes.cpp contiene el punto de entrada ZmqStreamInit
cmake/              opciones de build y dependencias
demo/               proyecto Godot de prueba
godot-cpp/          submódulo
```
