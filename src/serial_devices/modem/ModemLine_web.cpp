#include "serial_devices/modem/ModemLine.hpp"

#include <SDL3/SDL.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <emscripten/websocket.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

/*
 * The browser delivers WebSocket events only to a thread that returns to its
 * event loop. The modem worker never does, so each socket lives on the main
 * browser thread: the worker posts work there and never waits for it, and the
 * main thread's callbacks fill a buffer that the worker reads.
 */

namespace {

struct Socket {
    enum State { CONNECTING, OPEN, CLOSED };

    std::mutex lock;
    State state = CONNECTING;
    std::deque<uint8_t> received;
    std::vector<uint8_t> unsent;
    bool send_posted = false;

    /* Main thread only. self keeps the object alive while the socket can call back. */
    EMSCRIPTEN_WEBSOCKET_T ws = 0;
    std::shared_ptr<Socket> self;
};

void run_on_main_thread(std::function<void()> fn) {
    auto *task = new std::function<void()>(std::move(fn));
    emscripten_proxy_async(emscripten_proxy_get_system_queue(), emscripten_main_runtime_thread_id(),
                           [](void *arg) {
                               auto *t = static_cast<std::function<void()> *>(arg);
                               (*t)();
                               delete t;
                           },
                           task);
}

/* Main thread: drop the browser socket and let the object go. */
void release(Socket *s) {
    if (s->ws > 0) {
        emscripten_websocket_close(s->ws, 1000, "");
        emscripten_websocket_delete(s->ws);
        s->ws = 0;
    }
    {
        std::lock_guard<std::mutex> g(s->lock);
        s->state = Socket::CLOSED;
    }
    std::shared_ptr<Socket> last = std::move(s->self);
}

bool on_open(int, const EmscriptenWebSocketOpenEvent *, void *user) {
    auto *s = static_cast<Socket *>(user);
    std::lock_guard<std::mutex> g(s->lock);
    s->state = Socket::OPEN;
    return true;
}

bool on_message(int, const EmscriptenWebSocketMessageEvent *e, void *user) {
    auto *s = static_cast<Socket *>(user);
    std::lock_guard<std::mutex> g(s->lock);
    s->received.insert(s->received.end(), e->data, e->data + e->numBytes);
    return true;
}

bool on_error(int, const EmscriptenWebSocketErrorEvent *, void *user) {
    release(static_cast<Socket *>(user));
    return true;
}

bool on_close(int, const EmscriptenWebSocketCloseEvent *, void *user) {
    release(static_cast<Socket *>(user));
    return true;
}

/* Main thread: send what the worker wrote since the last send. */
void send_unsent(Socket *s) {
    std::vector<uint8_t> bytes;
    {
        std::lock_guard<std::mutex> g(s->lock);
        bytes.swap(s->unsent);
        s->send_posted = false;
    }
    if (s->ws > 0 && !bytes.empty()) {
        emscripten_websocket_send_binary(s->ws, bytes.data(), static_cast<uint32_t>(bytes.size()));
    }
}

class WebSocketLine : public ModemLine {
    public:
        WebSocketLine(std::shared_ptr<Socket> socket, std::string url)
            : socket_(std::move(socket)), peer_(std::move(url)) {}

        ~WebSocketLine() override {
            run_on_main_thread([s = socket_] { release(s.get()); });
        }

        int read(uint8_t *buf, int len) override {
            std::lock_guard<std::mutex> g(socket_->lock);
            if (socket_->received.empty()) {
                return socket_->state == Socket::CLOSED ? -1 : 0;
            }
            int n = 0;
            while (n < len && !socket_->received.empty()) {
                buf[n++] = socket_->received.front();
                socket_->received.pop_front();
            }
            return n;
        }

        /* Bytes written before the main thread sends go out as one message. */
        bool write(const uint8_t *buf, int len) override {
            std::lock_guard<std::mutex> g(socket_->lock);
            if (socket_->state == Socket::CLOSED) {
                return false;
            }
            socket_->unsent.insert(socket_->unsent.end(), buf, buf + len);
            if (!socket_->send_posted) {
                socket_->send_posted = true;
                run_on_main_thread([s = socket_] { send_unsent(s.get()); });
            }
            return true;
        }

        const std::string &peer() const override { return peer_; }

    private:
        std::shared_ptr<Socket> socket_;
        std::string peer_;
};

} // namespace

std::unique_ptr<ModemLine> modem_dial_tcp(const std::string &host, int port) {
    printf("ModemDevice: %s:%d: the browser build dials only ws:// and wss://\n", host.c_str(), port);
    return nullptr;
}

std::unique_ptr<ModemLine> modem_dial_websocket(const std::string &url) {
    printf("ModemDevice: Connecting to %s\n", url.c_str());
    auto socket = std::make_shared<Socket>();
    socket->self = socket;
    run_on_main_thread([s = socket, url] {
        EmscriptenWebSocketCreateAttributes attr;
        emscripten_websocket_init_create_attributes(&attr);
        attr.url = url.c_str();
        /* Synchronet sends 8-bit telnet data in binary frames only for this subprotocol. */
        attr.protocols = "binary";
        attr.createOnMainThread = true;
        s->ws = emscripten_websocket_new(&attr);
        if (s->ws <= 0) {
            release(s.get());
            return;
        }
        const pthread_t here = EM_CALLBACK_THREAD_CONTEXT_CALLING_THREAD;
        emscripten_websocket_set_onopen_callback_on_thread(s->ws, s.get(), on_open, here);
        emscripten_websocket_set_onmessage_callback_on_thread(s->ws, s.get(), on_message, here);
        emscripten_websocket_set_onerror_callback_on_thread(s->ws, s.get(), on_error, here);
        emscripten_websocket_set_onclose_callback_on_thread(s->ws, s.get(), on_close, here);
    });

    auto line = std::make_unique<WebSocketLine>(socket, url);
    // Wait for the connection (30 second timeout, as for TCP).
    for (int waited = 0; waited < 30000; waited += 10) {
        Socket::State state;
        {
            std::lock_guard<std::mutex> g(socket->lock);
            state = socket->state;
        }
        if (state == Socket::OPEN) {
            return line;
        }
        if (state == Socket::CLOSED) {
            break;
        }
        SDL_Delay(10);
    }
    printf("ModemDevice: Failed to connect to %s\n", url.c_str());
    return nullptr;
}

std::unique_ptr<ModemListener> modem_listen_tcp(uint16_t) {
    return nullptr;
}
