#ifndef STREAM_ZMQSUBSCRIBER_HPP
#define STREAM_ZMQSUBSCRIBER_HPP

// godot
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

// std
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * Recibe mensajes de un socket ZeroMQ SUB en un hilo propio y los entrega
 * al hilo principal de Godot mediante señales.
 *
 * Un mensaje de un frame se entrega con topic vacío. En mensajes de varios
 * frames, el primero es el topic y el último es el payload.
 *
 * Usa la C-API de libzmq. El contexto se crea en `start()` y se destruye en
 * `stop()` o el destructor; el socket se crea, se usa y se cierra dentro del
 * hilo de recepción. La conversión a tipos de Godot y la emisión de señales
 * ocurren solo en `_process`, nunca en el hilo de red.
 */
class ZmqSubscriber : public godot::Node
{
    GDCLASS(ZmqSubscriber, godot::Node)

public:
    ZmqSubscriber();
    ~ZmqSubscriber() override;

    void _exit_tree() override;
    void _process(double delta) override;
    void _ready() override;

    bool autoStart() const;
    bool bindSocket() const;
    int64_t droppedCount() const;
    godot::String endpoint() const;
    bool isRunning() const;
    int maxQueue() const;

    void setAutoStart(bool autoStart);
    void setBindSocket(bool bindSocket);
    void setEndpoint(const godot::String &endpoint);
    void setMaxQueue(int maxQueue);
    void setTopic(const godot::String &topic);

    /**
     * Crea el contexto y lanza el hilo de recepción. No hace nada si ya
     * está activo.
     */
    void start();

    /**
     * Detiene el hilo, cierra el socket y destruye el contexto.
     */
    void stop();

    godot::String topic() const;

protected:
    static void _bind_methods();

private:
    /**
     * Elemento de la cola entre el hilo de red y el hilo principal.
     */
    struct Message
    {
        std::vector<uint8_t> payload;
        std::string topic;
    };

    std::atomic<bool> alive_;
    bool autoStart_;
    bool bindSocket_;
    void *context_;
    std::atomic<int64_t> dropped_;
    godot::String endpoint_;
    std::deque<std::string> errors_;
    int maxQueue_;
    std::deque<Message> messages_;
    mutable std::mutex mutex_;
    std::atomic<bool> stopRequested_;
    std::thread thread_;
    godot::String topic_;

    /**
     * Encola un error para emitirlo en el hilo principal.
     */
    void enqueueError(const std::string &message);

    /**
     * Encola un mensaje y descarta el más antiguo si se supera `maxQueue`.
     */
    void enqueueMessage(Message &&message, int maxQueue);

    /**
     * Cuerpo del hilo: crea, usa y cierra el socket. Recibe copias de la
     * configuración para no leer propiedades desde otro hilo.
     */
    void runThread(void *context, const std::string &endpoint, const std::string &topic, bool bind, int maxQueue);
};

inline bool ZmqSubscriber::autoStart() const { return autoStart_; }
inline bool ZmqSubscriber::bindSocket() const { return bindSocket_; }
inline int64_t ZmqSubscriber::droppedCount() const { return dropped_.load(); }
inline godot::String ZmqSubscriber::endpoint() const { return endpoint_; }
inline bool ZmqSubscriber::isRunning() const { return alive_.load(); }
inline int ZmqSubscriber::maxQueue() const { return maxQueue_; }
inline void ZmqSubscriber::setAutoStart(bool autoStart) { autoStart_ = autoStart; }
inline void ZmqSubscriber::setBindSocket(bool bindSocket) { bindSocket_ = bindSocket; }
inline void ZmqSubscriber::setEndpoint(const godot::String &endpoint) { endpoint_ = endpoint; }
inline void ZmqSubscriber::setTopic(const godot::String &topic) { topic_ = topic; }
inline godot::String ZmqSubscriber::topic() const { return topic_; }

#endif
