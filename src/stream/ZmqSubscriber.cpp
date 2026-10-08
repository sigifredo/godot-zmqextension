// own
#include <stream/ZmqSubscriber.hpp>

// godot
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// std
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

// zmq
#include <zmq.h>

using namespace godot;

namespace
{
    // Mensajes máximos que se entregan por llamada a _process.
    const int MaxMessagesPerFrame = 16;

    // Espera máxima de zmq_poll en milisegundos; acota la latencia de stop().
    const long PollTimeoutMs = 100;
}

ZmqSubscriber::ZmqSubscriber()
{
    alive_ = false;
    autoStart_ = true;
    bindSocket_ = false;
    context_ = nullptr;
    dropped_ = 0;
    endpoint_ = "tcp://127.0.0.1:5555";
    maxQueue_ = 2;
    stopRequested_ = false;
}

ZmqSubscriber::~ZmqSubscriber()
{
    stop();
}

void ZmqSubscriber::_exit_tree()
{
    stop();
}

void ZmqSubscriber::_process(double delta)
{
    std::vector<Message> messages;
    std::vector<std::string> errors;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        while (!messages_.empty() && static_cast<int>(messages.size()) < MaxMessagesPerFrame)
        {
            messages.push_back(std::move(messages_.front()));
            messages_.pop_front();
        }

        while (!errors_.empty())
        {
            errors.push_back(std::move(errors_.front()));
            errors_.pop_front();
        }
    }

    for (const std::string &error : errors)
        emit_signal("error_occurred", String::utf8(error.c_str(), error.size()));

    for (const Message &message : messages)
    {
        PackedByteArray payload;
        payload.resize(static_cast<int64_t>(message.payload.size()));
        std::memcpy(payload.ptrw(), message.payload.data(), message.payload.size());
        emit_signal("message_received", String::utf8(message.topic.c_str(), message.topic.size()), payload);
    }
}

void ZmqSubscriber::_ready()
{
    set_process(true);

    if (autoStart_)
        start();
}

void ZmqSubscriber::setMaxQueue(int maxQueue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    maxQueue_ = std::max(1, maxQueue);
}

void ZmqSubscriber::start()
{
    if (thread_.joinable())
    {
        if (alive_.load())
            return;

        // El hilo terminó por un error fatal: se recupera antes de relanzar.
        stop();
    }

    context_ = zmq_ctx_new();

    if (!context_)
    {
        String message = String("ZmqSubscriber: zmq_ctx_new falló: ") + String::utf8(zmq_strerror(zmq_errno()));
        UtilityFunctions::push_error(message);
        emit_signal("error_occurred", message);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
        errors_.clear();
    }

    dropped_ = 0;
    stopRequested_ = false;
    alive_ = true;

    const std::string endpoint = endpoint_.utf8().get_data();
    const std::string topic = topic_.utf8().get_data();
    const bool bind = bindSocket_;
    const int maxQueue = maxQueue_;
    void *context = context_;

    thread_ = std::thread([this, context, endpoint, topic, bind, maxQueue]()
    {
        runThread(context, endpoint, topic, bind, maxQueue);
        alive_ = false;
    });

    UtilityFunctions::print("ZmqSubscriber: iniciado en ", endpoint_, bind ? " (bind)" : " (connect)");
}

void ZmqSubscriber::stop()
{
    stopRequested_ = true;

    if (thread_.joinable())
        thread_.join();

    if (context_)
    {
        zmq_ctx_term(context_);
        context_ = nullptr;
    }

    alive_ = false;
}

void ZmqSubscriber::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("start"), &ZmqSubscriber::start);
    ClassDB::bind_method(D_METHOD("stop"), &ZmqSubscriber::stop);
    ClassDB::bind_method(D_METHOD("is_running"), &ZmqSubscriber::isRunning);
    ClassDB::bind_method(D_METHOD("get_dropped_count"), &ZmqSubscriber::droppedCount);

    ClassDB::bind_method(D_METHOD("set_endpoint", "endpoint"), &ZmqSubscriber::setEndpoint);
    ClassDB::bind_method(D_METHOD("get_endpoint"), &ZmqSubscriber::endpoint);
    ClassDB::bind_method(D_METHOD("set_topic", "topic"), &ZmqSubscriber::setTopic);
    ClassDB::bind_method(D_METHOD("get_topic"), &ZmqSubscriber::topic);
    ClassDB::bind_method(D_METHOD("set_bind_socket", "bind_socket"), &ZmqSubscriber::setBindSocket);
    ClassDB::bind_method(D_METHOD("get_bind_socket"), &ZmqSubscriber::bindSocket);
    ClassDB::bind_method(D_METHOD("set_max_queue", "max_queue"), &ZmqSubscriber::setMaxQueue);
    ClassDB::bind_method(D_METHOD("get_max_queue"), &ZmqSubscriber::maxQueue);
    ClassDB::bind_method(D_METHOD("set_auto_start", "auto_start"), &ZmqSubscriber::setAutoStart);
    ClassDB::bind_method(D_METHOD("get_auto_start"), &ZmqSubscriber::autoStart);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "endpoint"), "set_endpoint", "get_endpoint");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "topic"), "set_topic", "get_topic");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bind_socket"), "set_bind_socket", "get_bind_socket");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_queue", PROPERTY_HINT_RANGE, "1,64,1,or_greater"), "set_max_queue", "get_max_queue");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_start"), "set_auto_start", "get_auto_start");

    ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "payload")));
    ADD_SIGNAL(MethodInfo("error_occurred", PropertyInfo(Variant::STRING, "message")));
}

void ZmqSubscriber::enqueueError(const std::string &message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.push_back(message);
}

void ZmqSubscriber::enqueueMessage(Message &&message, int maxQueue)
{
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back(std::move(message));

    while (static_cast<int>(messages_.size()) > maxQueue)
    {
        messages_.pop_front();
        dropped_++;
    }
}

void ZmqSubscriber::runThread(void *context, const std::string &endpoint, const std::string &topic, bool bind, int maxQueue)
{
    void *socket = zmq_socket(context, ZMQ_SUB);

    if (!socket)
    {
        enqueueError(std::string("zmq_socket falló: ") + zmq_strerror(zmq_errno()));
        return;
    }

    // Cada mensaje puede ocupar dos frames (topic y payload).
    int linger = 0;
    int highWaterMark = std::max(1, maxQueue) * 2;
    zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(socket, ZMQ_RCVHWM, &highWaterMark, sizeof(highWaterMark));

    bool ready = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, topic.data(), topic.size()) == 0;

    if (!ready)
    {
        enqueueError(std::string("ZMQ_SUBSCRIBE falló: ") + zmq_strerror(zmq_errno()));
    }
    else if ((bind ? zmq_bind(socket, endpoint.c_str()) : zmq_connect(socket, endpoint.c_str())) != 0)
    {
        ready = false;
        enqueueError(std::string(bind ? "zmq_bind" : "zmq_connect") + " falló en '" + endpoint + "': " + zmq_strerror(zmq_errno()));
    }

    while (ready && !stopRequested_.load())
    {
        zmq_pollitem_t item = {socket, 0, ZMQ_POLLIN, 0};
        const int polled = zmq_poll(&item, 1, PollTimeoutMs);

        if (polled < 0)
        {
            if (zmq_errno() == EINTR)
                continue;

            enqueueError(std::string("zmq_poll falló: ") + zmq_strerror(zmq_errno()));
            break;
        }

        if (polled == 0)
            continue;

        std::vector<std::vector<uint8_t>> frames;
        bool failed = false;
        int more = 1;

        while (more)
        {
            zmq_msg_t frame;
            zmq_msg_init(&frame);

            if (zmq_msg_recv(&frame, socket, 0) < 0)
            {
                if (zmq_errno() != EINTR)
                    enqueueError(std::string("zmq_msg_recv falló: ") + zmq_strerror(zmq_errno()));

                zmq_msg_close(&frame);
                failed = true;
                break;
            }

            const uint8_t *data = static_cast<const uint8_t *>(zmq_msg_data(&frame));
            frames.emplace_back(data, data + zmq_msg_size(&frame));
            zmq_msg_close(&frame);

            size_t moreSize = sizeof(more);

            if (zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &moreSize) != 0)
                more = 0;
        }

        if (failed || frames.empty())
            continue;

        Message message;
        message.payload = std::move(frames.back());

        if (frames.size() > 1)
            message.topic.assign(frames.front().begin(), frames.front().end());

        enqueueMessage(std::move(message), maxQueue);
    }

    zmq_close(socket);
}
