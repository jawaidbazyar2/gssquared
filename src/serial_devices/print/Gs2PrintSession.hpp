#pragma once

#include <cstdint>
#include <memory>

/**
 * One gs2print job. The SerialDevice worker calls every method.
 * GS2 binds an AF_UNIX socket, writes a .gs2print ticket, and asks the OS to
 * open that ticket without waiting. Guest bytes are queued here for the sidecar.
 */
class Gs2PrintSession {
public:
    Gs2PrintSession();
    ~Gs2PrintSession();

    Gs2PrintSession(const Gs2PrintSession &) = delete;
    Gs2PrintSession &operator=(const Gs2PrintSession &) = delete;

    /** Bind, write the ticket, and spawn the OS open. Worker thread. */
    void begin();
    /** Same byte the capture file just stored. No-op once the job stopped waiting. */
    void write(uint8_t byte);
    /** Accept, flush, reap the OS open, and apply the connect timeout. */
    void poll();
    /** Close the socket, unlink it, and remove the ticket. */
    void end();

    /** One-shot: the open failed or nobody connected. */
    bool take_app_missing();
    /** One-shot: the sidecar closed its end. */
    bool take_peer_closed();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
