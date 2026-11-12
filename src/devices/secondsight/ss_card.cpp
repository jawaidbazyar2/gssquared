/*
 *   Second Sight is a video card with its own Z8S180 processor and memory.
 *   The U7 Lattice chip watches Apple bus accesses. It captures video writes
 *   and asks the U17 copy controller to put them in the card's static RAM
 *   (SRAM). Other accesses become events: requests for the firmware to handle.
 *   The Oak OTI-087X Video Graphics Array (VGA) controller turns video
 *   memory written by the firmware into pixels.
 *
 *   This file joins those parts. The machine's memory mapper calls observe(); the
 *   card runs its processor and video clocks up to each Apple access. Reads
 *   can return the byte held in the U3 Apple data-out latch. A latch holds a
 *   value until its next load. A pending event can hold RDY (Ready) low to
 *   make the Apple wait before finishing another access.
 *
 *   Mega II is the IIgs chip providing the Apple II memory and expansion bus.
 *   Host ticks count the Apple 14.31818 MHz clock, about 70 ns per tick.
 *   A slow Apple bus cycle normally takes fourteen ticks. PHI is the Z180
 *   processor clock. A T-state is one PHI period; a half-PHI is the interval
 *   between successive rising and falling edges. PH0 is the Apple bus clock.
 *   ACK acknowledges a captured host event. RUN permits another event;
 *   STALL holds the Apple access until the acknowledgement has settled.
 *   BUSREQ asks the Z180 to give up its bus; BUSACK says it has done so. DMA
 *   (direct memory access) copies bytes without executing a CPU instruction.
 *   DREQ is a request input to one of the Z180's two DMA channels.
 *
 *   IRQ is an interrupt request. Card-to-Apple IRQ delivery can be late:
 *   the worker records edges, but the host collects and applies them only
 *   at synchronization or presentation points. The host CPU is not rewound
 *   to an edge's timestamp. Relaxed synchronization therefore preserves edge
 *   order without promising cycle-exact interrupt delivery.
 *   RTS is the serial request-to-send output, wired here to select ROM
 *   (read-only memory).
 *   EPROM is the erasable read-only chip containing the firmware. EEPROM
 *   is electrically erasable memory holding the video configuration.
 *
 *   The application owns SSCard. The worker alone runs chip state while a job
 *   is outstanding. Host reads and reset first synchronize(); presentation may
 *   instead claim stopped Oak state without an exact host-frame fence.
 *   Sources: Second Sight manual, card operation; Jawaid
 *   Bazyar's ROM_BEEF.asm (Reset, ClearScreen and host-command routines), at
 *   https://github.com/jawaidbazyar2/secondsight . Zilog UM005004-0918 pp. 16-30
 *   and 95-113 describe processor bus ownership and DMA. Board grant lengths
 *   below are scheduling choices; those Zilog pages do not specify card delays.
 *   Board analysis: https://github.com/Webifi/second-sight-reference,
 *   docs/07-glue-logic.md, docs/05-z180.md, docs/04-capture-logic.md,
 *   docs/reference/oti087-registers.md and
 *   data/lattice/Lattice_original-BE8C.ldf (the chip's equation listing).
 */

#include "ss_card.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <array>
#include <atomic>
#include <utility>
#ifdef __EMSCRIPTEN__
#include <emscripten/threading.h>
#endif

// A transfer's first observable phase precedes its falling-edge sample. The
// model's PH0 high is seven ticks before the falling edge (slot_bus.hpp).
static uint64_t slot_high(uint64_t fall) { return fall >= 7 ? fall - 7 : 0; }

// The worker owns card state while consuming the transfer stream. The host
// drains preceding entries for inspection or claims stopped Oak for drawing.
// IRQ collection uses its own lock and can occur while the worker continues.
// SDL windows and the Apple CPU stay on the host thread.
struct SSCard::Worker {
    // One posted Apple access or host time, in emulated-time order. A plain
    // transfer needs no answer. Otherwise the card runs through ticks and,
    // with access, applies that access too; its caller waits for completion,
    // so access and held stay valid until then.
    struct Entry {
        slot_bus_transfer_t transfer;
        slot_bus_transfer_t *access;
        uint64_t *held;
        uint64_t ticks;
        bool plain;
        // A write whose only possible answer is its RDY hold: the result goes
        // to held_ticks, then held_sequence, since the host may not wait for it.
        bool conditional;
    };
    // Transfers present pins at PH0 high; stop-only entries name their stop tick.
    static uint64_t input_time(Entry const &entry) {
        return entry.plain || entry.access || entry.conditional ? slot_high(entry.transfer.ticks) : entry.ticks;
    }
    // A bounded single-producer, single-consumer ring: the host thread alone
    // writes entries and posted, the card thread alone advances consumed and
    // completed. Release/acquire on the counters hands over the entries, the
    // results and all card state written before completed.
    static constexpr size_t RING_SIZE = 4096;
    std::array<Entry, RING_SIZE> ring;
    alignas(64) std::atomic<uint64_t> posted{0};
    alignas(64) std::atomic<uint64_t> consumed{0};
    alignas(64) std::atomic<uint64_t> completed{0};
    // The host's current bus time: every access before it is posted, so
    // the card may run through it.
    alignas(64) std::atomic<uint64_t> host_time{0};
    alignas(64) std::atomic<uint64_t> held_sequence{0};
    std::atomic<uint64_t> held_ticks{0};
    std::atomic<uint64_t> horizon{0};
    // Worker-only next-entry index and latest host time whose inputs are known.
    uint64_t next = 0, known = 0;
    // Short bus and input waits stay awake for this many empty checks.
    // Full-state fences sleep immediately while the host owns the card.
    static constexpr unsigned IDLE_CHECKS = 1U << 18;
    // 64 bus cycles of 14 host ticks: about 63 microseconds.
    static constexpr uint64_t TIME_STEP = 64 * 14;
    static constexpr uint64_t EVENT_STEP = 14;
    alignas(64) std::atomic<bool> sleeping{false};
    std::atomic<bool> stop{false};
    // A reset requests cancellation at the current exact fence. detached
    // means no instruction handler remains live on the worker stack.
    std::atomic<bool> mutation{false}, detached{true};
    // Only the renderer requests/owns this handoff; the worker publishes Ready
    // after finishing chip work at its current scheduler edge. No pixel copy.
    enum PresentationState { Idle, Requested, Ready, Owned };
    std::atomic<PresentationState> presentation{Idle};
    uint64_t presentation_ticks = 0;
    std::mutex mutex;
    std::condition_variable wake;
    // Apple IRQ edges carry card timestamps and stay in card order. The host
    // collects them at synchronization or presentation; until then an edge
    // may be delivered later than it occurred in emulated time.
    std::mutex irq_mutex;
    std::vector<std::pair<Z180Time, bool>> irq;
    std::thread thread;
    const std::thread::id host_thread = std::this_thread::get_id();
    std::exception_ptr error;
};

// Spin-wait hint for a short cross-thread wait; it reads no clock.
static inline void cpu_relax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#endif
}

// Connect the processor pins to the board decode and VGA chip. The serial RTS
// output selects the ROM overlay, so its callback must update direct-read pages.
SSCard::SSCard(const uint8_t *image, uint64_t rate, SSCapture::LatticeMap lattice_map, Presentation presentation, bool int0_pullup)
    : capture(lattice_map), host_lattice_map(lattice_map), int0_pullup(int0_pullup), presentation_mode(presentation), ticks_per_second(rate) {
    // image supplies exactly ROM_SIZE bytes; rate is host ticks per second.
    // Keep a truncated integer tick period, matching Z180Time::from_ticks.
    // Zero rate is handled as an unreachable deadline by transfer_deadline().
    if (uint32_t(rate))
        transfer_period = Z180Time::units_per_second / uint32_t(rate);
    std::memcpy(rom.data(), image, rom.size());
    // SD7:0 is this bus; SD15:8 is open. Oak's scan-test convention borrows
    // the byte already retained by every CPU access, including direct reads.
    // Second-sight-reference docs/reference/oti087-registers.md,
    // extended register $03.
    vga.bind_system_data(bus_data);
    transfers.reserve(QUEUE_SIZE);
    // The CPU has no knowledge of Apple addresses or video chips. These
    // callbacks translate its physical memory and I/O cycles into card actions.
    // An interrupt acknowledge reads whatever byte the card bus last held.
    Z180::Bus bus;
    bus.memory_read = [this](uint32_t a) { return memory_read(a); };
    bus.memory_write = [this](uint32_t a, uint8_t d) { memory_write(a, d); };
    bus.io_read = [this](uint16_t p) { return io_read(p); };
    bus.io_write = [this](uint16_t p, uint8_t d) { io_write(p, d); };
    bus.irq_ack = [this] { return interrupt_bus_byte(); };
    bus.clock_changed = [this](uint32_t hz) { clock_changed(hz); };
    cpu = std::make_unique<Z180>(std::move(bus), EXTAL_HZ, Z180::Silicon::K);
    cpu->serial(0).rts_handler = [this](bool level) {
        rom_overlay = level;
        update_direct_reads();
    };
    cpu->set_bus_wait([this](uint32_t a, bool write, bool io) {
        return io ? 0U : extra_wait(write ? Z180::Access::MemoryWrite : Z180::Access::MemoryRead, a);
    });
    // IORQ is the Z180 I/O-request signal. Its select and release edges
    // matter separately from the data read/write (UM0050 pp. 16-17).
    cpu->set_io_strobe([this](uint32_t port, bool write, uint8_t data, bool asserted) {
        if ((port & 0xf8) != 0xa0)
            return;
        ax_selected = asserted;
        unsigned reg = port & 7;
        if (reg == 0) {
            // BCLK, the U4 Z180 I/O decoder latch clock, rises at the final
            // write-data edge. This also applies
            // when relocated internal I/O owns the CPU's data transfer.
            // Apple reads from the select edge on wait for that edge.
            if (!asserted && write) {
                host_output = data;
            }
            ack_selected = asserted;
            phase(phase_high, card_time());
            if (!asserted)
                cpu->request_yield();
        }
        if (!asserted)
            return;
        if (reg >= 4)
            ax_nibble = capture.status(reg, 0) & 15;
        else
            strobe(reg, write ? data : bus_data);
    });
    // The U17 copy controller writes only after the CPU has surrendered the
    // bus. The CPU then releases BUSREQ at the end of /WR without a scheduler
    // return if it can.
    cpu->set_busack([this](bool asserted) {
        if (!asserted)
            return;
        Z180Time const now = card_time();
        advance_copy(now);
        copy_grant(now, now);
        if (!schedule_copy_release(now))
            cpu->request_yield();
    });
    cpu->set_board_edge([this] {
        copy_in_cpu_edge = true;
        advance_copy(copy_cpu_edge);
        copy_in_cpu_edge = false;
    });
    vga.cint_callback = [this](bool asserted) { cpu->set_input(Z180::Input::INT0, this->int0_pullup && asserted); };
    reset(0, true);
}

// Join the worker before its bus callbacks or card storage can be destroyed.
SSCard::~SSCard() {
    if (worker) {
        worker->stop.store(true, std::memory_order_seq_cst);
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->wake.notify_one();
        }
        if (worker->thread.joinable())
            worker->thread.join();
    }
}

// Start the card execution thread once. It runs entries posted by
// observe(), dispatch() and catch_up(). The browser's prestarted pthread pool
// owns the same worker and uses the same bus meeting points.
void SSCard::start_worker() {
    if (worker)
        return;
    worker = std::make_unique<Worker>();
    consumer_limit = Worker::RING_SIZE;
    host_pending = capture.pending;
    host_irq = apple_irq;
    worker->thread = std::thread([this] {
#ifdef __EMSCRIPTEN__
        // Give browser performance traces the same identifiable device owner.
        emscripten_set_thread_name(pthread_self(), "Second Sight");
#endif
        try {
            stream_entries();
        } catch (...) {
            Worker &w = *worker;
            w.error = std::current_exception();
            // Hand failed requests back so their callers rethrow the retained
            // error. Invariant failures must not strand a host result wait.
            while (!w.stop.load(std::memory_order_acquire)) {
                const uint64_t posted = w.posted.load(std::memory_order_acquire);
                w.consumed.store(posted, std::memory_order_release);
                w.completed.store(posted, std::memory_order_release);
                w.held_ticks.store(NO_TICKS, std::memory_order_relaxed);
                w.held_sequence.store(posted, std::memory_order_release);
                w.mutation.store(false, std::memory_order_release);
                w.sleeping.store(true, std::memory_order_seq_cst);
                std::unique_lock<std::mutex> lock(w.mutex);
                w.wake.wait(lock, [&] {
                    return w.posted.load(std::memory_order_acquire) != posted ||
                           w.mutation.load(std::memory_order_acquire) || w.stop.load(std::memory_order_acquire);
                });
                w.sleeping.store(false, std::memory_order_relaxed);
            }
        }
    });
}

// Run the Z180 on one worker stack. A bus meeting updates the board and
// returns another grant, rather than unwinding an unfinished instruction.
void SSCard::stream_entries() {
    Worker &w = *worker;
    Worker::Entry entry{};
    bool active = false, started = false, high = false, held = false;
    bool stopping = false, returning = false;
    uint64_t fall = 0, first = 0;

    // Rendering may take the current scan at a scheduler edge, including an
    // idle edge. Release/acquire transfers Oak ownership; the host never waits
    // here. Cancellation leaves all queued bus inputs in their original order.
    auto present = [&]() {
        if (w.presentation.load(std::memory_order_acquire) != Worker::Requested) return;
        advance_vga();
        w.presentation_ticks = card_time().floor_ticks(ticks_per_second);
        auto expected = Worker::Requested;
        if (!w.presentation.compare_exchange_strong(expected, Worker::Ready,
                std::memory_order_release, std::memory_order_relaxed)) return;
        w.sleeping.store(true, std::memory_order_seq_cst);
        std::unique_lock<std::mutex> lock(w.mutex);
        w.wake.wait(lock, [&] {
            const auto state = w.presentation.load(std::memory_order_acquire);
            // A new request may replace Idle before the worker wakes. It no
            // longer owns this handoff and must return to process that request.
            return (state != Worker::Ready && state != Worker::Owned) ||
                   w.stop.load(std::memory_order_acquire) || w.mutation.load(std::memory_order_acquire);
        });
        w.sleeping.store(false, std::memory_order_relaxed);
    };

    // Once completed is published, the host may inspect the card. Keep
    // every chip read on this side of that ownership hand-off.
    auto park = [&](bool mutation_complete = false, bool idle_fence = false) {
        const uint64_t time = w.host_time.load(std::memory_order_acquire);
        // Time publication alone supplies no new bus input. Accumulate a
        // useful grant while caught up; a posted transfer still wakes at once.
        // Follow a pending event every slot cycle so ACK stays close to the host.
        const uint64_t resume_time = time + (capture.pending ? Worker::EVENT_STEP : Worker::TIME_STEP);
#ifdef GS2_SS_TESTING
        const bool executing = cpu->park();
#endif

        w.completed.store(w.next, std::memory_order_release);
        if (mutation_complete) w.mutation.store(false, std::memory_order_release);
        unsigned idle = 0;
        while (w.posted.load(std::memory_order_acquire) == w.next &&
               w.host_time.load(std::memory_order_acquire) < resume_time &&
               !w.mutation.load(std::memory_order_acquire) &&
               !w.stop.load(std::memory_order_acquire)) {
            present();
            if (!idle_fence && ++idle < Worker::IDLE_CHECKS) {
                cpu_relax();
                continue;
            }
            w.sleeping.store(true, std::memory_order_seq_cst);
            std::unique_lock<std::mutex> lock(w.mutex);
            w.wake.wait(lock, [&] {
                return w.posted.load(std::memory_order_acquire) != w.next ||
                       w.host_time.load(std::memory_order_acquire) >= resume_time ||
                       w.mutation.load(std::memory_order_acquire) ||
                       w.presentation.load(std::memory_order_acquire) == Worker::Requested ||
                       w.stop.load(std::memory_order_acquire);
            });
            w.sleeping.store(false, std::memory_order_relaxed);
            idle = 0;
        }
#ifdef GS2_SS_TESTING
        cpu->unpark(executing);
#endif
    };
    auto scheduler = [&]() -> uint64_t {
        for (;;) {
            const auto now = card_time();
            service_card_deadlines(now);

            if (w.stop.load(std::memory_order_acquire)) {
                stopping = returning = true;
                cpu->stop_after_instruction();
            }
            if (w.mutation.load(std::memory_order_acquire)) {
                cpu->cancel_for_reset();
                return cpu->half_cycles();
            }
            if (returning) {
                auto end = std::min({now + cpu->phi_time(16), vga_deadline(vga.next_event()), copy_wake()});
                return cpu->half_cycles() + std::max<uint64_t>(1, (end - now).ceil_ticks(cpu_hz * 2));
            }

            present();

            // Acquire time before entries: publication of time follows every
            // earlier host input. Inputs are never ordered by arrival time.
            w.known = std::max(w.known, w.host_time.load(std::memory_order_acquire));
            const uint64_t posted = w.posted.load(std::memory_order_acquire);

            if (!active && w.next != posted) {
                entry = w.ring[w.next % Worker::RING_SIZE];
                const uint64_t input = Worker::input_time(entry);
                w.known = std::max(w.known, input);
                if (input_passed(input))
                    throw std::runtime_error("Second Sight: streamed transfer high already passed");

                // The copied entry no longer needs its ring slot. Return
                // slots in groups too, or immediately for a waiting caller.
                if ((w.next + 1) % PUBLISH_ENTRIES == 0 || !entry.plain)
                    w.consumed.store(w.next + 1, std::memory_order_release);
                active = true;
                started = high = held = false;
                fall = first = entry.transfer.ticks;
            }
            // An active entry supplies its full bus cycle, including the
            // repeated falls of a hold. The ordinary grant also rounds to
            // the first half-PHI at its deadline; that is not run-ahead.
            if (active && (entry.plain || entry.access || entry.conditional)) {
                const auto rise = transfer_deadline(slot_high(fall));
                const auto sample = transfer_deadline(fall);
                if (!high && now >= rise) {
                    transfer_rise(entry.transfer, now, rise, started, high);
                }
                if (high && now >= sample) {
                    if (transfer_fall(entry.transfer, sample, fall, held)) {
                        const uint64_t extra = fall - first;
                        stall_ticks += extra;
                        if (held) progress(fall);
                        if (entry.plain && extra)
                            throw std::runtime_error("Second Sight: queued RDY access escaped arbitration");
                        if (entry.access) {
                            *entry.access = entry.transfer;
                            *entry.held = extra;
                        }
                        ++w.next;

                        active = false;
                        if (entry.conditional) {
                            w.held_ticks.store(extra, std::memory_order_relaxed);
                            w.held_sequence.store(w.next, std::memory_order_release);
                        } else if (!entry.plain) {
                            advance_vga();
                            park();
                        }
                        continue;
                    }
                    if (!held) { held = true; ++stalls; }
                    progress(fall);
                    fall = slot_bus_next_fall(fall);
                    high = false;
                }
            } else if (active && now >= transfer_deadline(entry.ticks)) {
                ++w.next;
                active = false;
                advance_vga();
                // A full-state fence hands the card to presentation, reset or
                // inspection. Sleep until the host resumes it; bus results
                // keep the short-wait policy above.
                park(false, true);
                continue;
            }

            auto end = transfer_deadline(w.known);

            if (active) {
                // RDY retains this same transfer; its later PH0 edges are
                // known even while the host is held (Apple IIgs Technical Note 68 Fig.2).
                end = entry.plain || entry.access || entry.conditional
                    ? transfer_deadline(high ? fall : slot_high(fall))
                    : transfer_deadline(entry.ticks);
            }
            if (end <= now) {
                park();
                continue;
            }
            end = limit_card_deadline(end);
            return cpu->half_cycles() + (end - now).ceil_ticks(cpu_hz * 2);
        }
    };

    for (;;) {
        w.detached.store(false, std::memory_order_relaxed);
        cpu->set_scheduler(scheduler);
        cpu->run_half_until(UINT64_MAX);
        cpu->set_scheduler({});
        if (!cpu->at_instruction_boundary())
            throw std::runtime_error("Second Sight: streaming returned inside an instruction");
        if (w.mutation.load(std::memory_order_acquire)) {
            // No time or bus edge was issued by the cancelled tail. The host
            // now resets the CPU at the same exact fence, with no snapshot.
            returning = false;
            w.detached.store(true, std::memory_order_release);
            park(true);
        }
        if (stopping) return;

    }
}

// The host owns an exact parked fence. Cancel the old handler without bus
// or time effects before applying reset; normal execution takes no snapshot.
void SSCard::prepare_reset() {
    synchronize();
    if (!worker || worker->detached.load(std::memory_order_acquire)) return;
    worker->mutation.store(true, std::memory_order_release);
    wake_card();
    while (worker->mutation.load(std::memory_order_acquire)) cpu_relax();
}

// Host thread: every slot bus access before ticks has been posted.
void SSCard::publish_time(uint64_t ticks) {
    // Virtual block devices issue a whole block without advancing the Apple
    // clock. Its serialized bus writes precede this publication on the card.
    ticks = std::max(ticks, posted_through);
    if (!worker || ticks <= published_time)
        return;
    published_time = ticks;
    // A possible event needs frequent time grants until its ACK is resolved.
    // Otherwise publish in 64-cycle groups to avoid sharing the atomic
    // host-time counter on every bus cycle.
    if (pending_time_grant && event_clear(ticks)) pending_time_grant = false;
    if (!pending_time_grant && ticks - shared_time < Worker::TIME_STEP) return;
    publish_entries();
    worker->host_time.store(ticks, std::memory_order_release);
    shared_time = ticks;
    wake_card();
}

// Publish every initialized slot before a time frontier or required result.
// The worker still applies each input at its original high and falling edge.
void SSCard::publish_entries() {
    if (announced == submitted) return;
    worker->posted.store(submitted, std::memory_order_seq_cst);
    announced = submitted;
}

// Wake after publishing work. The wait uses this mutex too, so notification
// cannot pass between the worker checking for work and going to sleep.
void SSCard::wake_card() {
    Worker &w = *worker;
    if (w.sleeping.load(std::memory_order_seq_cst)) {
        std::lock_guard<std::mutex> lock(w.mutex);
        w.wake.notify_one();
    }
}

// Host thread: no Lattice event can be pending until the host posts another
// possible event. The card sets pending only for a host event, so a clear flag
// after the last posted possible event stays clear.
bool SSCard::event_clear(uint64_t ticks) const {
    const uint64_t state = event_line.state.load(std::memory_order_acquire);
    return !(state & 1) && (state >> 1) >= event_ticks &&
           event_line.ack_ticks.load(std::memory_order_relaxed) <= slot_high(ticks);
}

// The next request can avoid an RDY hold only if ACK is released and RUN
// recovers before its PH0 high. Pending alone is not the RDY pin; see the chip's
// equation listing, data/lattice/Lattice_original-BE8C.ldf, lines 424-445.
void SSCard::publish_event_state(uint64_t accepted_ticks) {
    event_accepted_tick = accepted_ticks;
    event_line.ack_ticks.store(event_ready_tick, std::memory_order_relaxed);
    const uint64_t value = accepted_ticks << 1 | uint64_t(!event_ready);
    if (event_line.state.load(std::memory_order_relaxed) != value)
        event_line.state.store(value, std::memory_order_release);
}

// Drain all submitted transfers before exposing card state to the host.
// Collect Apple IRQ edges into host storage. The caller delivers them at
// its chosen host sample; this function does not call the Apple IRQ handler.
void SSCard::synchronize() {
    end_presentation();
    if (!worker || (synchronized == submitted && synchronized_time == published_time))
        return;
    // A timed entry stops the card exactly at its time, also when the
    // remainder is shorter than its time step. The card then waits, since
    // the host publishes nothing more.
    if (!stopping_post || posted_through < published_time)
        post(slot_bus_transfer_t{}, nullptr, nullptr, std::max(posted_through, published_time), false);
    wait_for(submitted);
    finish_synchronize();
}

// After the completion wait, surface worker failures and take the stopped
// capture state and queued IRQ edges. This runs on the host thread.
void SSCard::finish_synchronize() {
    synchronized = submitted;
    synchronized_time = published_time;
    // The completed counter is also the memory hand-off for error, capture
    // and IRQ (interrupt request) changes made by the worker.
    if (worker->error) {
        stop_after_failure();
        return;
    }
    host_pending = capture.pending;
    collect_irq();
}

// A failed check means the card's timing state is no longer valid. Report it
// instead of ending the program: the Apple, its monitor and the debugger keep
// running, and the card stays stopped and off the bus.
void SSCard::stop_after_failure() {
    if (card_failed) return;
    card_failed = true;
    try {
        std::rethrow_exception(worker->error);
    } catch (const std::exception &error) {
        failure_text = error.what();
    } catch (...) {
        failure_text = "unknown error";
    }
    host_pending = false;
    irq_edges.clear();
    if (host_irq && irq_handler) irq_handler(false);
    host_irq = false;
}

// Move published IRQ edges without taking ownership of live card state.
// The short lock protects the vector; callbacks run later without that lock.
void SSCard::collect_irq() {
    if (!worker) return;
    std::vector<std::pair<Z180Time, bool>> edges;
    {
        std::lock_guard<std::mutex> lock(worker->irq_mutex);
        edges.swap(worker->irq);
    }
    irq_edges.insert(irq_edges.end(), edges.begin(), edges.end());
}

// Request a stopped scan for this host frame. Latest mode does not wait here;
// it delivers only the IRQ edges the worker has published so far.
void SSCard::request_presentation(uint64_t ticks, bool exact) {
    end_presentation();
    if (card_failed) return;
    if (!worker || presentation_mode == Presentation::Exact || exact) {
        catch_up(ticks, exact);
        return;
    }
    publish_time(ticks);
    publish_entries();
    // A cancelled request may still be finishing its Oak preparation. Do not
    // let an old completed synchronization permit a later inspection.
    synchronized_time = NO_TICKS;
    stopping_post = false;
    collect_irq();
    deliver_irq(ticks);
    worker->presentation.store(Worker::Requested, std::memory_order_release);
    wake_card();
}

// Claim the requested scan, or return false so the renderer keeps its texture.
// Success transfers Oak ownership until end_presentation.
bool SSCard::begin_presentation(uint64_t ticks, bool exact) {
    if (card_failed) return false;
    if (!worker || presentation_mode == Presentation::Exact || exact) {
        catch_up(ticks);
        return true;
    }
    auto expected = Worker::Ready;
    if (!worker->presentation.compare_exchange_strong(expected, Worker::Owned,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        end_presentation();
        return false;
    }
    // A slow card cannot leave a monitor arbitrarily far behind. This bound
    // is a presentation policy, not a change to either emulated oscillator.
    if (ticks > worker->presentation_ticks &&
        ticks - worker->presentation_ticks > ticks_per_second / 60) {
        end_presentation();
        catch_up(ticks);
    }
    return true;
}

// Cancel a request or return Oak ownership. Wake the worker only if it had
// already offered or handed over a stopped scan. Host thread only.
void SSCard::end_presentation() {
    if (!worker || worker->presentation.load(std::memory_order_relaxed) == Worker::Idle) return;
    const auto previous = worker->presentation.exchange(Worker::Idle, std::memory_order_acq_rel);
    if (previous == Worker::Ready || previous == Worker::Owned) wake_card();
}

// Post one entry to the card thread; wait only while the ring is full.
void SSCard::post(slot_bus_transfer_t const &transfer, slot_bus_transfer_t *access, uint64_t *held, uint64_t ticks,
                  bool plain, bool conditional) {
    Worker &w = *worker;
    // A frame or inspection follows every byte of a virtual-device burst,
    // even when those bytes share one Apple tick. Keep the card fence after
    // their assigned falls; this does not add time to the Apple clock.
    if (!plain && !access && !conditional)
        ticks = std::max(ticks, posted_through);
    // The producer owns this cached free-space limit. A consumer load is
    // needed only after using all slots already returned, not at each access.
    if (submitted == consumer_limit) {
        publish_entries();
        wake_card();
        do {
            consumer_limit = w.consumed.load(std::memory_order_acquire) + Worker::RING_SIZE;
            if (submitted == consumer_limit) cpu_relax();
        } while (submitted == consumer_limit);
    }
    w.ring[submitted % Worker::RING_SIZE] = {transfer, access, held, ticks, plain, conditional};
    posted_through = std::max(posted_through, ticks);
    stopping_post = !plain && !conditional;
    ++submitted;
    if (!plain || submitted - announced >= PUBLISH_ENTRIES) {
        publish_entries();
        wake_card();
    }
}

// Wait until the card thread has completed entry count; no clock is read.
void SSCard::wait_for(uint64_t count) {
    while (worker->completed.load(std::memory_order_acquire) < count) {
        consume_progress();
        cpu_relax();
    }
    consume_progress();
}

// Keep IRQ edges in order while the worker owns the card. The host applies
// collected edges at a later synchronization or presentation sample, so
// interrupt delivery is not cycle-exact across the two processors.
void SSCard::publish_irq(bool asserted) {
    if (worker && worker->host_thread != std::this_thread::get_id()) {
        Worker &w = *worker;
        const auto timestamp = card_time();
        std::lock_guard<std::mutex> lock(w.irq_mutex);
        w.irq.push_back({timestamp, asserted});
    }
    else
        irq_edges.push_back({card_time(), asserted});
}

// Apply collected edges through this absolute host tick. Future edges stay
// queued. Earlier edges are delivered here, not at their original CPU cycle;
// relaxed synchronization can therefore delay card-to-Apple interrupts.
void SSCard::deliver_irq(uint64_t ticks) {
    if (irq_edges.empty()) return;
    // This runs on the host after taking the published edge list.
    // Convert with the fixed host rate without reading live processor state.
    const auto sample = Z180Time::from_ticks(ticks, ticks_per_second);
    size_t count = 0;
    while (count < irq_edges.size() && irq_edges[count].first <= sample) {
        host_irq = irq_edges[count].second;
        if (irq_handler) irq_handler(host_irq);
        ++count;
    }
    irq_edges.erase(irq_edges.begin(), irq_edges.begin() + count);
}

// Submit captured bus transfers through the same scheduler in either mode.
// An access that returns data or stretches RDY must wait for its result.
uint64_t SSCard::dispatch(uint64_t ticks, bool wait, slot_bus_transfer_t *access) {
    uint64_t held = 0;
    if (worker) {
        // ticks is an absolute host-clock count. wait requests a full drain;
        // access must be paired with wait=true because its result pointers
        // remain valid only until this call returns. held is in host ticks.
        post(access ? *access : slot_bus_transfer_t{}, access, access ? &held : nullptr, ticks, false);
        if (wait)
            synchronize();
        // A read may also finish after its requested fall because of RDY.
        // Later fences must retain that accepted time, as conditional writes do.
        if (access)
            posted_through = std::max(posted_through, access->ticks + held);
    } else {
        run_transfers(transfers, access ? slot_high(access->ticks) : ticks);
        transfers.clear();
        if (access)
            held = process(*access);
    }
    return held;
}

// Replace the EPROM, Lattice map and INT0 pull-up as a power cycle; keep the
// display and nonvolatile EEPROM objects that the application already owns.
void SSCard::change_hardware(uint64_t ticks, const uint8_t *image, SSCapture::LatticeMap lattice_map, bool int0_pullup) {
    if (card_failed) return;
    catch_up(ticks);
    if (card_failed) return;
    prepare_reset();
    std::memcpy(rom.data(), image, rom.size());
    capture.lattice_map = lattice_map;
    host_lattice_map = lattice_map;
    this->int0_pullup = int0_pullup;
    reset(ticks, true);
}

// Apple reset restarts the Z180, but does not clear the Lattice chip or host
// latches. Only power-on clears SRAM and those board-level retained outputs.
void SSCard::reset(uint64_t ticks, bool power_on) {
    // A stopped card has no valid timing state to restart from.
    if (card_failed) return;
    synchronize();
    if (!power_on)
        catch_up(ticks);
    if (card_failed) return;
    prepare_reset();
    transfers.clear();
    queued_event = false;
    next_transfer_tick = ticks;
    phase_transfer = {};
    phase_decode = {};
    phase_high = false;
    ready_since = Z180Time::from_ticks(ticks, ticks_per_second);
    event_accepted_tick = ticks;
    event_ready_tick = ticks;
    irq_edges.clear();
    // ticks anchors the reset to host time. A power cycle initializes RAM
    // and retained card latches; an Apple reset only restarts the processor.
    // Zero-filled SRAM is a deterministic emulator choice, not power-up data.
    if (power_on) {
        sram.fill(0);
        capture.power_on();
        vga_half = linear = rgb = apple_irq = false;
        bus_data = 0xff;
        bus_valid = false;
        host_output = 0;
        copies = events = stalls = stall_ticks = 0;
    }
    // U7 Lattice chip and the U4 Z180 I/O decoder/U15 Z180 memory decoder outputs have no Apple reset input.
    rom_overlay = true;
    bus_request = ax_selected = ack_selected = false;
    copy_phase = CopyPhase::idle;
    copy_pending = false;
    copy_deadline = Z180Time::never;
    cpu->reset();
    // The card holds DMA request 0 active. Fixed-source memory fills need
    // that request (UM0050 pp. 105-109; ROM_BEEF.asm ClearScreen).
    cpu->set_input(Z180::Input::DREQ0, true);
    update_direct_reads();
    host_epoch = Z180Time::from_ticks(ticks, ticks_per_second) - cpu->time();
    cpu_hz = cpu->clock_hz();
    update_periods();
    vga.reset();
    vga.output_divider_w(rgb);
    vga.advance(double(cpu_time()));
    drive_event();
    host_pending = capture.pending;
    host_irq = apple_irq;
    event_ready = !capture.pending && !ack_selected && capture.run;
    pending_time_grant = !event_ready;
    publish_event_state(ticks);
    if (irq_handler)
        irq_handler(apple_irq);
}

// hz is the new PHI frequency in cycles per second. Discard cached wait
// lengths and yield so the card can convert deadlines at that rate.
// Zilog UM0050 pp. 34-41 describe the clock multiplier and divider.
void SSCard::clock_changed(uint32_t hz) {
    cpu_hz = hz;
    wait_seconds[0] = wait_seconds[1] = -1;
    if (cpu) {
        update_periods();
        cpu->request_yield();
    }
}

void SSCard::update_periods() {
    tick_period = cpu->cycles_to_time(1);
    phi_period = cpu->phi_time(1);
    copy_write_period = cpu->phi_time(COPY_WRITE_PHI);
}

// Run only to the next host, VGA or copy controller edge. Acknowledgement
// waits stop at the port strobe so the Apple can resume at the proper PH0 edge.
void SSCard::run_to(Z180Time deadline, bool awaiting_ack) {
    // deadline is absolute emulated time. awaiting_ack allows an earlier
    // return as soon as the firmware releases its acknowledge port. Each
    // iteration stops at the earliest video edge, copy completion or deadline;
    // no chip may run past a change that another chip must observe.
    while (card_time() < deadline && (!awaiting_ack || capture.pending || ack_selected)) {
        double const vga_event = vga.next_event();
        Z180Time const event_deadline = vga_deadline(vga_event);
        Z180Time end = std::min({deadline, event_deadline, copy_wake()});
        if (end > card_time())
            cpu->run_half_until(cpu->half_cycles() + (end - card_time()).ceil_ticks(cpu_hz * 2));
        advance_copy(card_time());
        // If the exact integer deadline has passed, retain the original VGA
        // double for dispatch; converting card time back must not round below it.
        vga.advance(std::max(double(cpu_time()), card_time() >= event_deadline ? vga_event : 0.0));
    }
}

// Expose only ROM/SRAM pages whose reads have no board-side decode action;
// the CPU must still update the retained data-bus byte on each direct read.
void SSCard::update_direct_reads() {
    for (unsigned page = 0; page < 4; ++page)
        cpu->set_direct_read_page(page, (rom_overlay && page < 2 ? rom.data() : sram.data()) + ((page & 1) << 16),
                                  bus_data, &bus_valid);
    for (unsigned page = 6; page < 8; ++page)
        cpu->set_direct_read_page(page, rom.data() + ((page & 1) << 16), bus_data, &bus_valid);
}

// Convert the U17 copy controller's release edge to a CPU half-PHI edge
// without rounding earlier; BUSREQ releases only after the SRAM write.
bool SSCard::schedule_copy_release(Z180Time now) {
    if (copy_phase != CopyPhase::write_start && copy_phase != CopyPhase::write_end)
        return false;
    Z180Time const release = copy_phase == CopyPhase::write_start ? copy_deadline + copy_write_period : copy_deadline;
    return schedule_copy_edge(release, now);
}

bool SSCard::schedule_copy_edge(Z180Time at, Z180Time now) {
    if (at <= now)
        return false;
    copy_cpu_edge = at;
    return cpu->schedule_board_edge(cpu->half_cycles() + (at - now).ceil_ticks(cpu_hz * 2));
}

// Bridge the VGA raster clock to integer CPU time. Cache by the exact double
// value so unchanged raster deadlines have identical rounding.
Z180Time SSCard::vga_deadline(double event) {
    // Timing-register writes and raster events can change the deadline. Its
    // binary64 value is the complete conversion input, including after reset.
    if (event_seconds != event) {
        event_seconds = event;
        event_time = Z180Time::from_seconds(event);
    }
    return event_time;
}

void SSCard::advance_vga() {
    const double event = vga.next_event();
    vga.advance(std::max(cpu_time(), card_time() >= vga_deadline(event) ? event : 0.0));
}

// Translate host ticks to card time using the same truncated oscillator
// period as from_ticks; batching must not change when a bus access occurs.
Z180Time SSCard::transfer_deadline(uint64_t ticks) {
    // Retain from_ticks' whole-second boundary and truncated tick period.
    // Consecutive captured bytes normally share this host-clock second.
    const uint32_t rate = ticks_per_second;
    if (!rate)
        return Z180Time::never;
    if (ticks < transfer_epoch_tick || ticks - transfer_epoch_tick >= rate) {
        const uint64_t seconds = ticks / rate;
        transfer_epoch_tick = seconds * rate;
        transfer_epoch_time = Z180Time(seconds);
    }
    return transfer_epoch_time + Z180Time(0, (ticks - transfer_epoch_tick) * transfer_period);
}

// U17 copy controller edges. See UM0050 pp. 28-30 for BUSREQ/BUSACK.
void SSCard::run_copy_edges(Z180Time now) {
    while (copy_deadline <= now) {
        Z180Time const edge = copy_deadline;
        switch (copy_phase) {
        case CopyPhase::synchronize:
            copy_phase = CopyPhase::busack;
            copy_deadline = Z180Time::never;
            bus_request = true;
            if (copy_in_cpu_edge)
                cpu->bus_request_edge();
            else
                cpu->bus_release_w(true);
            if (cpu->bus_acknowledged())
                copy_grant(edge, card_time());
            break;
        case CopyPhase::write_start:
            // /WR starts: write what the U5/U6 address latches and the U2
            // Apple data-in latch hold now, not what they held at CATCH
            // (second-sight-reference docs/07-glue-logic.md, "Copy timing
            // budget"). The latch drives the card data bus.
            copy_phase = CopyPhase::write_end;
            copy_pending = false;
            copy_deadline = edge + copy_write_period;
            sram[copy_address] = capture.host_input;
            bus_data = capture.host_input;
            bus_valid = true;
            ++copies;
            break;
        case CopyPhase::write_end:
            copy_phase = CopyPhase::idle;
            copy_deadline = Z180Time::never;
            bus_request = false;
            cpu->bus_release_w(false);
            if (copy_pending)
                copy_capture(edge, card_time());
            break;
        default:
            throw std::runtime_error("Second Sight: U17 copy controller edge without a pending CATCH");
        }
    }
}

// A CATCH while the U17 copy controller is busy sets its one pending edge;
// later edges merge. This is an approximation: the controller is undumped.
void SSCard::copy_capture(Z180Time when, Z180Time now) {
    copy_pending = true;
    if (copy_phase == CopyPhase::idle) {
        copy_phase = CopyPhase::synchronize;
        copy_deadline = next_phi_edge(when, now);
        // Inside board_edge the CPU continues without a scheduler call, so an
        // edge it cannot hold needs one.
        if (!schedule_copy_edge(copy_deadline, now) && copy_in_cpu_edge)
            cpu->request_yield();
    }
}

void SSCard::copy_grant(Z180Time when, Z180Time now) {
    if (copy_phase != CopyPhase::busack)
        return;
    copy_phase = CopyPhase::write_start;
    copy_deadline = next_phi_edge(when, now);
}

// Count from the CPU's own PHI phase so the edge converts back to the same
// half-PHI deadline exactly.
Z180Time SSCard::next_phi_edge(Z180Time when, Z180Time now) const {
    if (cpu->phi_phase()) {
        Z180Time const last = now - tick_period;
        return last > when ? last : now + tick_period;
    }
    return now > when ? now : now + phi_period;
}

// The U7 Lattice chip pending-event output feeds both Z180 INT1 and DMA request 1;
// acknowledging the host port must drop both signals together.
void SSCard::drive_event() {
    cpu->set_input(Z180::Input::INT1, capture.pending);
    cpu->set_input(Z180::Input::DREQ1, capture.pending);
}

// Translate a Z180 byte address to the address presented to Oak. An aperture
// is the memory range through which the CPU reaches video RAM. Board latches
// provide its upper address bits; Oak's own bank registers act afterwards.
uint32_t SSCard::oak_address(uint32_t address) const {
    return (linear ? 0x100000 : 0) | (vga_half ? 0x80000 : 0) | (address & 0x7ffff);
}

// Convert Oak readiness to external WAIT duration. The memory decoder
// wait overlaps programmed CPU waits rather than adding a second wait train.
unsigned SSCard::extra_wait(Z180::Access access, uint32_t address) {
    // VGA register I/O needs one wait, which the Z180's own I/O wait always gives.
    if (access != Z180::Access::MemoryRead && access != Z180::Access::MemoryWrite)
        return 0;
    address = cpu->a18_tout_address(address);
    if (address < 0x80000)
        return 0;
    // U15 Z180 memory decoder waits overlap DCNTL waits; UM0050 p. 30 samples WAIT at T2.
    // Return full PHI periods of external wait. Round the half-PHI count up
    // to avoid allowing a CPU access before the video chip is ready.
    // Without the Oak's DRAM timeline, use the reference's no-contention
    // counts at 18 MHz (second-sight-reference docs/05-z180.md,
    // "A functional timing estimate"): reads 5,
    // writes 5, or 3 with the write cache on. The U15 Z180 memory decoder
    // forces its own count from its decode, so that count is a floor in PHI.
    const unsigned write = access == Z180::Access::MemoryWrite;
    const unsigned fixed = write && vga.write_cache_enabled() ? 3 : 5;
    const double seconds = std::max(vga.access_wait(write), double(fixed) / 18'000'000);
    const uint32_t rate = cpu->clock_hz();
    if (wait_seconds[write] != seconds || wait_rate[write] != rate) {
        const auto ticks = cpu->time_to_cycles(Z180Time::from_seconds(seconds));
        wait_phi[write] = std::max(FORCED_MEMORY_WAITS, unsigned((ticks + 1) / 2));
        wait_seconds[write] = seconds;
        wait_rate[write] = rate;
    }
    return wait_phi[write];
}

// Read a 20-bit Z180 physical byte address. Return the byte driven by ROM,
// SRAM or Oak; an unselected device leaves the previous bus byte in place.
// ROM_BEEF.asm Reset uses the overlay to copy ROM into RAM, then removes it.
// Map: 00000-1FFFF ROM or SRAM, 20000-3FFFF SRAM mirror, 40000-5FFFF
// half-select latch, 60000-7FFFF ROM mirror, 80000-FFFFF Oak aperture.
uint8_t SSCard::memory_read(uint32_t address) {
    address = cpu->a18_tout_address(address & 0xfffff);
    if ((address < 0x20000 && rom_overlay) || (address >= 0x60000 && address < 0x80000)) {
        bus_data = rom[address & 0x1ffff];
        bus_valid = true;
    } else if (address < 0x40000) {
        bus_data = sram[address & 0x1ffff];
        bus_valid = true;
    } else if (address < 0x60000) {
        vga_half = address & 0x10000;
    } else {
        vga.advance(double(cpu_time()));
        if (vga.memory_read_selected(oak_address(address))) {
            bus_data = vga.memory_read(oak_address(address)).data;
            bus_valid = true;
        }
    }
    return bus_data;
}

// Write data to a 20-bit physical byte address, using the map above. A CPU
// write drives the data bus even if ROM is selected and discards the store.
// Address bit 18 can instead be the timer output pin (UM0050 pp. 157-164).
void SSCard::memory_write(uint32_t address, uint8_t data) {
    address = cpu->a18_tout_address(address & 0xfffff);
    bus_data = data;
    bus_valid = true;
    if ((address < 0x20000 && rom_overlay) || (address >= 0x60000 && address < 0x80000))
        return;
    if (address < 0x40000) {
        sram[address & 0x1ffff] = data;
    }
    else if (address < 0x60000)
        vga_half = address & 0x10000;
    else {
        vga.advance(double(cpu_time()));
        vga.memory_write(oak_address(address), data);
    }
}

// Decode the low nibble of Z180 ports $A0-$AF: $A0 acknowledges the host,
// $A1 bit 2 inhibits copies and bit 3 enables IIgs bank selection. $A2/$A3
// have no action here. $A4/$A5 read and clear dirty-band bits; $A6/$A7 read
// the event address. $A8/$A9 select VGA/RGB timing; $AA/$AB disable/enable
// linear addressing; $AC/$AD assert/release Apple IRQ; $AE/$AF do nothing.
// port is the low nibble, data is the driven byte. Port selection itself
// acts, on reads too. See ROM_BEEF.asm Reset and its IN0/OUT0 $A0-$AD uses.
void SSCard::strobe(unsigned port, uint8_t data) {
    switch (port) {
        case 0: {
            // Debugger/direct port accesses have no surrounding IORQ callback.
            // Give those accesses the same select/release edges at this time.
            const bool direct = !ack_selected;
            if (direct) ack_selected = true;
            bool const was_pending = capture.pending;
            phase(phase_high, card_time());
            drive_event();
            if (was_pending)
                cpu->request_yield();
            if (direct) {
                ack_selected = false;
                phase(phase_high, card_time());
            }
            break;
        }
        case 1:
            capture.configure(data);
            // The decoder inputs are live even during a held Apple phase.
            phase_decode = capture.transfer(phase_transfer);
            phase(phase_high, card_time());
            break;
        case 8:
        case 9:
            rgb = port & 1;
            vga.output_divider_w(rgb);
            cpu->request_yield();
            break;
        case 10:
        case 11:
            linear = port & 1;
            break;
        case 12:
        case 13:
            if (apple_irq != !(port & 1)) {
                apple_irq = !(port & 1);
                publish_irq(apple_irq);
            }
            break;
    }
}

// Return the Apple data-in latch at $A0 or capture status at $A4-$A7.
// Other VGA-selected ports go to Oak; unselected bits retain the data bus.
uint8_t SSCard::io_read(uint16_t port) {
    vga.advance(double(cpu_time()));
    if ((port & 0xf0) == 0xa0) {
        const unsigned reg = port & 15;
        const uint8_t data = reg == 0 ? capture.host_input
                             : reg >= 4 && reg <= 7
                                 ? (ax_selected ? (bus_data & 0xf0) | ax_nibble : capture.status(reg, bus_data))
                                 : bus_data;
        if (reg != 0 || !ack_selected)
            strobe(reg, bus_data);
        bus_data = data;
        if (reg == 0 || (reg >= 4 && reg <= 7))
            bus_valid = true;
    } else if (vga.io_read_selected(port)) {
        bus_data = vga.io_r(port);
        bus_valid = true;
    }
    return bus_data;
}

// Apply board port strobes or forward VGA/DAC register writes to Oak.
// The $A0 output latch uses the IORQ release edge during real CPU cycles.
void SSCard::io_write(uint16_t port, uint8_t data) {
    vga.advance(double(cpu_time()));
    bus_data = data;
    bus_valid = true;
    if ((port & 0xf0) == 0xa0) {
        const unsigned reg = port & 15;
        // Direct fixture/debug accesses complete here. Machine cycles load
        // U3 Apple data-out latch and release ACK through the IORQ edge callback.
        if (reg == 0 && !ack_selected) {
            host_output = data;
        }
        if (reg >= 4 && reg <= 7)
            bus_data = ax_selected ? (data & 0xf0) | ax_nibble : capture.status(reg, data);
        else if (reg != 0 || !ack_selected)
            strobe(reg, data);
    } else {
        vga.io_w(port, data);
        cpu->request_yield();
    }
}

// Bring the card to the host transfer time before evaluating its bus pins.
uint64_t SSCard::process(slot_bus_transfer_t &transfer) {
    return accept_transfer(transfer, transfer_deadline(transfer.ticks));
}

// Advance the two card-owned deadlines in the same order at every scheduler edge.
inline void SSCard::service_card_deadlines(Z180Time now) {
    advance_copy(now);
    if (cpu->bus_acknowledged())
        schedule_copy_release(now);
    else if (copy_phase == CopyPhase::synchronize)
        schedule_copy_edge(copy_deadline, now);
    // cpu_time() can round below now, so one pass can leave a later Oak event
    // whose deadline has also passed. A grant limited to it would be empty.
    for (double event = vga.next_event(); now >= vga_deadline(event); event = vga.next_event())
        vga.advance(std::max(cpu_time(), event));
}

// A scheduled board edge already supplies its own deadline.
inline Z180Time SSCard::limit_card_deadline(Z180Time end) {
    return std::min({end, vga_deadline(vga.next_event()), copy_wake()});
}

// Apply U7 Lattice chip pin changes, then pass a copy edge to the U17 copy
// controller. CATCH does not depend on RUN: see the chip's equation listing,
// data/lattice/Lattice_original-BE8C.ldf, blocks A7/A2/B1. A held phase
// can pair a new address with the old byte in the U2 Apple data-in latch.
void SSCard::phase(bool high, Z180Time timestamp) {
    // U17 copy controller edges before this one act on the latches as they were.
    advance_copy(timestamp);
    const bool was_stalled = capture.stall;
    phase_high = high;
    const auto edge = capture.phase(phase_transfer, phase_decode, high, ack_selected);
    if (was_stalled && !capture.stall) ready_since = timestamp;
    if (edge.interrupt) drive_event();
    const bool ready = !capture.pending && !ack_selected && capture.run;
    if (ready && !event_ready) event_ready_tick = timestamp.ceil_ticks(ticks_per_second);
    event_ready = ready;
    publish_event_state(event_accepted_tick);
    if (edge.event) ++events;
    if (edge.copy) {
        copy_address = (phase_decode.bank ? 0x10000 : 0) | phase_transfer.address;
        Z180Time const now = card_time();
        copy_capture(timestamp, now);
        // The first PHI edge after CATCH can be the current half-PHI itself.
        advance_copy(now);
    }
}

// One physical PH0 rise, shared by streamed and direct transfers.
inline void SSCard::transfer_rise(slot_bus_transfer_t &transfer, Z180Time now, Z180Time rise, bool &started, bool &high) {
    if (!started) {
        phase_transfer = transfer;
        phase_decode = capture.transfer(transfer);
        phase(false, now);
        started = true;
    }
    phase(true, rise);
    high = true;
}

// Sample the U3 Apple data-out latch and retire one physical fall. A held period may repeat.
inline bool SSCard::transfer_fall(slot_bus_transfer_t &transfer, Z180Time sample, uint64_t fall, bool held) {
    const bool ready = !capture.stall &&
        (!held || ready_since + Z180Time(0, 60'000'000'000) <= sample);
    if (phase_decode.drive) {
        transfer.data = host_output;
        transfer.driven = true;
    }
    phase(false, sample);
    if (ready) {
        event_accepted_tick = fall;
        publish_event_state(fall);
    }
    return ready;
}

// Publish a held period whose card-side pin changes have all been resolved.
// The host can advance motherboard time through this fall. IRQ edges have
// been recorded, but this publication neither collects nor delivers them.
void SSCard::progress(uint64_t ticks) {
    if (worker && worker->thread.get_id() == std::this_thread::get_id()) {
        worker->horizon.store(ticks, std::memory_order_release);
    } else if (held_progress) {
        held_progress(ticks);
    }
}

bool SSCard::input_passed(uint64_t ticks) {
    // Exact runs round to the first half-PHI at or after a pin edge. A soft
    // retirement may pass that edge while still in the same whole host tick.
    return card_time() >= transfer_deadline(ticks) + cpu->cycles_to_time(1);
}

// transfer.ticks identifies its first PH0 fall; timestamp is that fall in
// card time. While RDY holds the access, address, data and the IIgs Fast
// Processor Interface bank remain presented for each following bus period.
// ACK, RUN/STALL release and the accepted fall are distinct. Apple IIgs
// Technical Note 68, Fig. 2 requires RDY settled 60 ns before the fall;
// a late release costs a period.
uint64_t SSCard::accept_transfer(slot_bus_transfer_t &transfer, Z180Time timestamp) {
    uint64_t fall = transfer.ticks;
    const uint64_t first = fall;
    const auto input = transfer_deadline(slot_high(fall));
    // run_to meets a pin edge at the first half-PHI at or after it. Anything
    // later has executed past an unseen input, not merely rounded its time.
    if (card_time() >= input + cpu->cycles_to_time(1))
        throw std::runtime_error("Second Sight: transfer high already passed: card " +
            std::to_string(card_time().floor_ticks(ticks_per_second)) + " high " +
            std::to_string(slot_high(fall)) + " address " + std::to_string(transfer.address));
    bool started = false, high = false, accepted = false, held = false;
    // PH0 edges are board deadlines, not processor returns. Keeping the
    // retained transfer on the scheduler stack preserves the current
    // instruction handler across RDY periods (Apple IIgs Technical Note 68, Fig. 2;
    // chip equation listing data/lattice/Lattice_original-BE8C.ldf,
    // lines 424-445).
    auto scheduler = [&]() -> uint64_t {
        const auto now = card_time();
        service_card_deadlines(now);
        const auto rise = transfer_deadline(slot_high(fall));
        if (!high && now >= rise) {
            transfer_rise(transfer, now, rise, started, high);
        }
        if (high && now >= timestamp) {
            if (transfer_fall(transfer, timestamp, fall, held)) {
                accepted = true;
                return cpu->half_cycles();
            }
            if (!held) { held = true; ++stalls; }
            progress(fall);
            fall = slot_bus_next_fall(fall);
            timestamp = transfer_deadline(fall);
            high = false;
        }
        auto end = limit_card_deadline(high ? timestamp : transfer_deadline(slot_high(fall)));
        return cpu->half_cycles() + (end > now ? (end - now).ceil_ticks(cpu_hz * 2) : 0);
    };
    cpu->set_scheduler(scheduler);
    try {
        // The scheduler ends only at the accepted fall; it never grants
        // execution beyond the next physical bus, copy or Oak edge.
        while (!accepted)
            cpu->run_half_until(UINT64_MAX);
    } catch (...) {
        cpu->set_scheduler({});
        throw;
    }
    cpu->set_scheduler({});
    if (!accepted)
        throw std::runtime_error("Second Sight: retained transfer ended before acceptance");
    advance_copy(card_time());
    advance_vga();
    if (held) progress(fall);
    const uint64_t extra = fall - first;
    stall_ticks += extra;
    return extra;
}

// Advance the host only through time with all earlier release/output edges resolved.
void SSCard::consume_progress() {
    const uint64_t ticks = worker->horizon.load(std::memory_order_acquire);
    if (ticks > delivered_horizon) {
        delivered_horizon = ticks;
        if (held_progress && (!host_clock_ticks || ticks > host_clock_ticks()))
            held_progress(ticks);
    }
}

// Meet the host at a frame/reset/debug boundary before reading shared state.
void SSCard::catch_up(uint64_t ticks, bool wait) {
    dispatch(ticks, wait);
    if (wait) deliver_irq(ticks);
    queued_event = false;
}

// Apply queued accesses at their high/falling phases on the active CPU
// stack. A possible hold leaves that stack before running the retained bus
// request; ordinary transfers keep the existing in-stack scheduler.
uint64_t SSCard::run_transfers(std::vector<slot_bus_transfer_t> &queue, uint64_t ticks) {
    size_t next = 0;
    const uint64_t target = queue.empty() ? ticks : std::max(ticks, queue.back().ticks);
    const auto deadline = transfer_deadline(target);
    bool high = false;
    while (next < queue.size() || card_time() < deadline) {
        bool blocked = false;
        auto scheduler = [&]() -> uint64_t {
            const auto now = card_time();
            service_card_deadlines(now);
            if (next < queue.size()) {
                auto &transfer = queue[next];
                const auto input = transfer_deadline(slot_high(transfer.ticks));
                if (!high && now >= input) {
                    // The in-stack path obeys the same first-half-PHI
                    // input deadline as accept_transfer below it.
                    if (now >= input + cpu->cycles_to_time(1))
                        throw std::runtime_error("Second Sight: queued transfer high already passed: card " +
                            std::to_string(now.floor_ticks(ticks_per_second)) + " high " +
                            std::to_string(slot_high(transfer.ticks)) + " address " + std::to_string(transfer.address) +
                            " entry " + std::to_string(next) + "/" + std::to_string(queue.size()) +
                            " target " + std::to_string(target) + " worker " + std::to_string(bool(worker)));
                    // ACK and feedback can make this access a held one.
                    // No PH0 edge has been applied yet, so process() can
                    // resume it without duplicating a copy or U2 Apple data-in latch clock.
                    if (capture.pending || capture.stall || ack_selected) {
                        blocked = true;
                        return cpu->half_cycles();
                    }
                    bool started = false;
                    transfer_rise(transfer, now, input, started, high);
                }
                if (high && now >= transfer_deadline(transfer.ticks)) {
                    if (capture.stall)
                        throw std::runtime_error("Second Sight: queued access acquired RDY");
                    transfer_fall(transfer, transfer_deadline(transfer.ticks), transfer.ticks, false);
                    ++next;
                    high = false;
                }
            }
            auto end = limit_card_deadline(deadline);
            if (next < queue.size())
                end = std::min(end, transfer_deadline(high ? queue[next].ticks : slot_high(queue[next].ticks)));
            return cpu->half_cycles() + (end > now ? (end - now).ceil_ticks(cpu_hz * 2) : 0);
        };
        cpu->set_scheduler(scheduler);
        try {
            const auto now = card_time();
            cpu->run_half_until(cpu->half_cycles() +
                (deadline > now ? (deadline - now).ceil_ticks(cpu_hz * 2) : 1));
            // A retirement can land exactly on a fall; settle that edge
            // before exposing its sampled byte, without running another instruction.
            scheduler();
        } catch (...) {
            cpu->set_scheduler({});
            throw;
        }
        cpu->set_scheduler({});
        advance_copy(card_time());
        advance_vga();
        if (blocked && process(queue[next++]))
            throw std::runtime_error("Second Sight: queued RDY access escaped arbitration");
    }
    run_to(deadline);
    // Published progress is only the whole-tick part. Late-input and saved
    // input decisions above retain the exact half-PHI time instead.
    return card_time().floor_ticks(ticks_per_second);
}

// Accept a slot bus transfer before the MMU completes it. Readback, IRQ and
// possible RDY holds drain the queue; independent writes may be batched.
uint64_t SSCard::observe(slot_bus_transfer_t &host_transfer) {
    if (card_failed) return 0;
    const uint16_t a = host_transfer.address;
    const bool c0 = (a & 0xff00) == 0xc000;
    const bool possible_copy = !host_transfer.read && ((a >= 0x400 && a < 0xc00) || (a >= 0x2000 && a < 0xa000));
    // The U7 Lattice chip acts only on $C0xx accesses and on writes to the
    // video memory ranges. No other cycle changes its state, so such a cycle
    // takes no card time. On a IIe these are most of the bus cycles.
    if (!c0 && !possible_copy && !host_irq) return 0;
    // Virtual disk DMA may submit a whole block at one host tick. The bus
    // can carry one byte per cycle: assign those bytes successive card-side
    // event times. This does not advance the Apple clock or drop a byte.
    slot_bus_transfer_t transfer = host_transfer;
    transfer.ticks = slot_bus_fall_at_or_after(std::max(transfer.ticks, next_transfer_tick));
    next_transfer_tick = slot_bus_next_fall(transfer.ticks);
    // These broad address checks decide when batching is safe. The U7
    // Lattice chip still performs the exact decode in accept_transfer().
    // A queued event may become pending, so the next possible copy or event
    // must wait for the worker before the Apple is allowed to continue.
    const bool host = (a & 0xfff0) == 0xc0b0;
    const bool possible_event = c0 && ((host && !(a & 8)) || (a >= 0xc050 && a <= 0xc057) || a == 0xc05e || a == 0xc05f ||
                                       (!transfer.read && ((a >= 0xc00c && a <= 0xc00f) || a == 0xc022 || a == 0xc029 ||
                                                           a == 0xc02b || (host_lattice_map == SSCapture::LatticeMap::V2 && a == 0xc034) ||
                                                           a == 0xc07e || a == 0xc07f)));
    // Card execution may acknowledge an event before host state catches up. Resolve
    // that at the next possible RDY access, before the Apple completes it.
    // With the card thread, read its live event state instead of the copy
    // taken at the last meeting: a copy or event waits only while an event
    // may still be unacknowledged at this access.
    bool maybe_pending = (possible_copy || possible_event) &&
                         (worker ? !event_clear(transfer.ticks) : (queued_event || capture.pending));
    // A host-port read returns the U3 Apple data-out latch, so it needs the
    // card at this time. A host-port write is an event: like a copy, it can
    // be held only while an earlier event is still pending.
    bool const answer = host && (transfer.read || !worker);
    bool const irq = host_irq;

    if (worker && maybe_pending && !answer && !irq && (possible_copy || possible_event)) {
        // Only the earlier event decides a hold. Post this access once; the
        // card has either taken that event before this time (no hold, so the
        // host continues at once) or returns the hold for this access.
        post(transfer, nullptr, nullptr, transfer.ticks, false, true);
        uint64_t const sequence = submitted;
        uint64_t held = 0;
        for (;;) {
            consume_progress();
            if (event_clear(transfer.ticks))
                break;
            if (worker->held_sequence.load(std::memory_order_acquire) >= sequence) {
                held = worker->held_ticks.load(std::memory_order_relaxed);
                // Only a failed card thread reports no hold result. The
                // synchronization records the failure; this access is not held.
                if (held == NO_TICKS) {
                    synchronize();
                    return 0;
                }
                break;
            }
            cpu_relax();
        }
        // The result's release store follows the accepted fall. Take its
        // progress time after acquiring that result, even if the earlier load
        // saw only an intermediate held phase (Apple IIgs Technical Note 68,
        // Ready signal).
        consume_progress();
        if (possible_event) {
            event_ticks = transfer.ticks;
            pending_time_grant = true;
        }
        // A later inspection must fence at the accepted fall after RDY.
        posted_through = std::max(posted_through, transfer.ticks + held);
        next_transfer_tick = slot_bus_next_fall(transfer.ticks + held);
        deliver_irq(transfer.ticks + held);
        return held;
    }
    if (answer || irq || (maybe_pending && (possible_copy || possible_event))) {
        const uint64_t held = dispatch(transfer.ticks, true, &transfer);
        if (card_failed) return 0;
        queued_event = false;
        if (possible_event) {
            event_ticks = transfer.ticks;
            pending_time_grant = true;
        }
        next_transfer_tick = slot_bus_next_fall(transfer.ticks + held);
        host_transfer.data = transfer.data;
        host_transfer.driven = transfer.driven;
        deliver_irq(transfer.ticks + held);
        return held;
    }
    if (worker) {
        post(transfer, nullptr, nullptr, transfer.ticks, true);
        if (possible_event) {
            event_ticks = transfer.ticks;
            pending_time_grant = true;
            // Start the event promptly. In particular the following byte of
            // a host word store may already need its acknowledgement.
            publish_entries();
            wake_card();
        }
    } else {
        if (transfers.size() == QUEUE_SIZE)
            // The incoming transfer is not in this queue yet. Its PH0 high
            // is the next input, so the drain must not pass it to the fall.
            dispatch(slot_high(transfer.ticks), false);
        transfers.push_back(transfer);
    }
    queued_event |= possible_event;
    return 0;
}

// The debugger reads the card's port as the Apple would see it, without the
// access itself: catch the card up to the current host time, then sample U3.
void SSCard::inspect(slot_bus_transfer_t &transfer, uint64_t ticks) {
    if (card_failed || (transfer.address & 0xfff0) != 0xc0b0) return;
    catch_up(ticks);
    if (card_failed) return;
    transfer.data = host_output;
    transfer.driven = true;
}
