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
 *   is outstanding. Host reads and reset synchronize; presentation may use
 *   the previous card frame while the worker finishes the next one.
 *   Sources: Second Sight manual, card operation; Jawaid
 *   Bazyar's ROM_BEEF.asm (Reset, ClearScreen and host-command routines), at
 *   https://github.com/jawaidbazyar2/secondsight . Zilog UM005004-0918 pp. 16-30
 *   and 95-113 describe processor bus ownership and DMA. Board grant lengths
 *   below are scheduling choices; those Zilog pages do not specify card delays.
 *   Board analysis: https://github.com/Webifi/second-sight-reference,
 *   docs/07-glue-logic.md, docs/05-z180.md and docs/04-capture-logic.md;
 *   chip equations: data/lattice/Lattice_original-BE8C.ldf.
 */

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "ss_capture.hpp"
#include "z180/Z180.hpp"
#include "oti087.hpp"

// Owns the physical card chips and their shared bus schedule. Host accessors
// synchronize before exposing worker-owned state.
class SSCard {
public:
    // Board EPROM and SRAM are each 128 KiB; address decode mirrors them.
    static constexpr size_t ROM_SIZE = 0x20000;
    static constexpr size_t SRAM_SIZE = 0x20000;

    // Inspection keeps an exact fence. A monitor can instead borrow the
    // latest card scan without waiting for the Apple frame deadline.
    enum class Presentation { Exact, Latest };

    // Copy ROM_SIZE bytes from rom; ticks_per_second is the host clock rate.
    // lattice_map selects the U7 Lattice chip equations used for capture.
    // int0_pullup adds the /INT0 pull-up that the stock board lacks.
    explicit SSCard(const uint8_t *rom, uint64_t ticks_per_second, SSCapture::LatticeMap lattice_map = SSCapture::LatticeMap::BE8C,
                    Presentation presentation = Presentation::Exact, bool int0_pullup = false);
    // Finish outstanding worker jobs before destroying the chips and callbacks.
    ~SSCard();
    // Start asynchronous card execution on native or browser pthread builds.
    void start_worker();
    // Stop after all submitted jobs and collect their IRQ edges. This alone
    // does not deliver them; catch_up(), observe() and presentation apply
    // collected edges through their requested host timestamp.
    void synchronize();
    // Reset at an absolute host tick; power_on also clears RAM and card latches.
    void reset(uint64_t ticks, bool power_on = false);
    // Install a ROM_SIZE-byte image, capture map and INT0 pull-up at ticks as a power cycle.
    void change_hardware(uint64_t ticks, const uint8_t *image, SSCapture::LatticeMap lattice_map, bool int0_pullup);
    // Observe one Apple access; update read data and return extra RDY wait ticks.
    uint64_t observe(slot_bus_transfer_t &transfer);
    // A debugger read of $C0B0-$C0BF gets the U3 Apple data-out latch at host
    // time ticks. It is not a bus cycle: no event, no copy and no RDY hold.
    void inspect(slot_bus_transfer_t &transfer, uint64_t ticks);
    // True after a card timing check failed on the card thread. The card then
    // stays stopped until the machine starts again: it holds no RDY, drives
    // no data and keeps its Apple IRQ released. The Apple keeps running.
    bool failed() const { return card_failed; }
    // The failed check's message, for the monitor, stderr and the debugger.
    const std::string &failure() const { return failure_text; }
    // Drain captured accesses and run the card through this absolute host tick.
    // wait=false lets independent Apple presentation overlap the remaining work;
    // the presenter must synchronize or claim stopped Oak before reading it.
    void catch_up(uint64_t ticks, bool wait = true);
    // Ask the worker to stop Oak while the Apple picture is drawn. ticks is
    // the absolute host time; exact forces a full synchronization. In Latest
    // mode this request also delivers already-published IRQ edges, possibly late.
    void request_presentation(uint64_t ticks, bool exact = false);
    // Return true only after claiming stopped Oak state. False means reuse
    // the previous texture. A scan over 1/60 second old forces synchronization.
    bool begin_presentation(uint64_t ticks, bool exact = false);
    // Release a claimed scan or cancel an unclaimed request; host thread only.
    void end_presentation();
    // Host thread, after every slot bus access before ticks has been observed:
    // let the card thread run through ticks without waiting for a request.
    void publish_time(uint64_t ticks);
    // CPU bus callbacks: physical 20-bit byte addresses, with device side effects.
    uint8_t memory_read(uint32_t address);
    // Drive data and store through the same physical address decoder as reads.
    void memory_write(uint32_t address, uint8_t data);
    // Read a Z180 I/O port, including board acknowledgement and status effects.
    uint8_t io_read(uint16_t port);
    // Write a byte to a Z180 I/O port; board ports decode their low address byte.
    void io_write(uint16_t port, uint8_t data);

    // Host inspection: wait before borrowing the capture state; do not retain
    // this reference across a later asynchronous submission.
    const SSCapture &capture_state() {
        synchronize();
        return capture;
    }
    // Borrow the stopped processor for debugger/fixture access after draining jobs.
    Z180 &processor() {
        synchronize();
        return *cpu;
    }
    // Borrow video state after draining jobs; display use is on the main thread.
    OTI087 &video() {
        synchronize();
        return vga;
    }
    // Borrow the stopped card RAM for inspection, indexed by physical SRAM byte.
    const std::array<uint8_t, SRAM_SIZE> &sram_data() {
        synchronize();
        return sram;
    }
    // The following observations require a stopped/synchronized card. This byte
    // is held by the U3 Apple data-out latch for Apple reads of $C0B0-$C0BF.
    uint8_t output_latch() const { return host_output; }
    // Interrupt acknowledge data: use all ones until any device drives the bus.
    uint8_t interrupt_bus_byte() const { return bus_valid ? bus_data : 0xff; }
    // Whether the retained card bus byte has a known driver since power-on.
    bool data_bus_driven() const { return bus_valid; }
    // U12 RGB timing logic combines the electrical sync pins in negative
    // logic: either low pulls XSYNC low (second-sight-reference
    // docs/07-glue-logic.md, "U12: AppleColor RGB timing").
    int composite_sync() { return vga.hsync_r() & vga.vsync_r(); }
    // Diagnostic totals: copied bytes, firmware events, held accesses, held ticks.
    uint64_t copies = 0, events = 0, stalls = 0, stall_ticks = 0;
    // Host-thread callback: true asserts the card interrupt on the Apple CPU.
    // This is delivery time, which may follow the recorded card-edge time.
    std::function<void(bool)> irq_handler;
    // Core-thread progress through resolved RDY falls; only autonomous
    // motherboard devices advance while the retained CPU access waits.
    std::function<void(uint64_t)> held_progress;
    // The host may retire a hold before consuming its worker notification.
    // Compare with the motherboard clock, not the last posted card access.
    std::function<uint64_t()> host_clock_ticks;

private:
    // 36 MHz board oscillator; Z8S180 reset divides EXTAL by two.
    static constexpr uint32_t EXTAL_HZ = 36'000'000;
    // U17 copy controller (undumped 18CV8), modeled using the copy timing
    // budget (second-sight-reference docs/07-glue-logic.md, "Copy timing
    // budget"): BUSREQ at the first PHI edge after CATCH, /WR at the first
    // PHI edge after BUSACK, then two PHI of
    // /WR, the shortest registered pulse meeting the SRAM's 60 ns at 18 MHz.
    static constexpr unsigned COPY_WRITE_PHI = 2;
    // U15 Z180 memory decoder (undumped) has no read/write input: three waits,
    // the least common count meeting the Oak's worst-case write at 18 MHz
    // (second-sight-reference docs/05-z180.md, "External waits and
    // worst-case margins").
    static constexpr unsigned FORCED_MEMORY_WAITS = 3;
    // Scheduler upper bound only; the acknowledge edge can end a slice sooner.
    static constexpr unsigned HANDSHAKE_SLICE_PHI = 128;
    std::array<uint8_t, ROM_SIZE> rom{};
    std::array<uint8_t, SRAM_SIZE> sram{};
    SSCapture capture;
    // Host-side decode must not read capture while the worker changes it.
    // The installed Lattice map changes only at a stopped hardware change.
    SSCapture::LatticeMap host_lattice_map;
    // Schematic: the /INT0 net joins only Z180 pin 10 and Oak CINT (pin 90),
    // with no pull-up, so on a stock card CINT never reaches INT0. Changes
    // only at a stopped hardware change.
    bool int0_pullup;
    const Presentation presentation_mode;
    OTI087 vga;
    std::unique_ptr<Z180> cpu;
    std::vector<slot_bus_transfer_t> transfers;
    // Host allocation/batch capacity, not a FIFO present on the card.
    static constexpr size_t QUEUE_SIZE = 512;
    struct Worker;
    std::unique_ptr<Worker> worker;
    static constexpr uint64_t NO_TICKS = UINT64_MAX;
    // Host thread: the latest posted entry stops the card at its time.
    bool stopping_post = false;
    // Host thread: set once from the worker's retained error; see failed().
    bool card_failed = false;
    std::string failure_text;
    // Record the worker's failure, release the Apple IRQ and stop the card.
    void stop_after_failure();
    // Publish ordinary entries together; a result or a time grant flushes
    // every earlier input first. This is host transport, not a card FIFO.
    static constexpr unsigned PUBLISH_ENTRIES = 32;
    void wake_card();
    void publish_entries();
    bool host_pending = false, host_irq = false;
    // Host-thread counts of entries posted to the card thread and of entries
    // whose results and IRQ edges the host has already taken.
    uint64_t submitted = 0, synchronized = 0;
    uint64_t announced = 0, consumer_limit = 0;
    // Host time last published to the card thread and last waited for, and
    // the entry count after the latest posted possible Lattice event.
    uint64_t published_time = 0, synchronized_time = 0;
    uint64_t shared_time = 0, delivered_horizon = 0;
    bool pending_time_grant = false;
    // Host tick of the latest posted possible Lattice event.
    uint64_t event_ticks = 0;
    // Card thread to host, on their own cache line so the host's checks do
    // not take the card's working fields from its core: the latest accepted
    // host tick << 1 | the pending flag, and the host tick, rounded up, of
    // the latest acknowledge.
    struct alignas(64) EventState {
        std::atomic<uint64_t> state{0};
        std::atomic<uint64_t> ack_ticks{0};
    } event_line;
    // Card thread: publish the pending flag after accepting at accepted_ticks.
    void publish_event_state(uint64_t accepted_ticks);
    // Latest host tick of any posted entry; the card runs at least that far.
    uint64_t posted_through = 0;
    // True when no Lattice event can be pending at a host access at ticks.
    bool event_clear(uint64_t ticks) const;
    // Working state used to publish a zero-hold result.
    uint64_t event_accepted_tick = 0, event_ready_tick = 0;
    bool event_ready = true;
    std::vector<std::pair<Z180Time, bool>> irq_edges;
    // Host thread: apply collected edges whose timestamps are at or before ticks.
    // Several late edges may be delivered before the next Apple instruction.
    void deliver_irq(uint64_t ticks);
    // Publish the resolved end of a held Apple bus period, in host ticks.
    void progress(uint64_t ticks);
    // Host thread: advance motherboard devices through published hold time.
    // IRQ collection and delivery are separate from this clock update.
    void consume_progress();
    bool queued_event = false;
    bool rom_overlay = true, vga_half = false, linear = false, rgb = false, apple_irq = false;
    uint8_t bus_data = 0xff, host_output = 0;
    bool bus_valid = false;
    uint64_t ticks_per_second;
    uint64_t next_transfer_tick = 0;
    bool bus_request = false, ax_selected = false, ack_selected = false;
    uint8_t ax_nibble = 0;
    // U5/U6 address latches with the bank bit, loaded by every rising CATCH.
    uint32_t copy_address = 0;
    // The U17 copy controller remembers one CATCH edge; copy_deadline is its
    // next registered edge.
    enum class CopyPhase : uint8_t { idle, synchronize, busack, write_start, write_end };
    CopyPhase copy_phase = CopyPhase::idle;
    bool copy_pending = false;
    // copy_cpu_edge is the copy controller edge the CPU calls back at
    // (BUSREQ or its release).
    Z180Time copy_deadline = Z180Time::never, copy_cpu_edge, host_epoch;
    bool copy_in_cpu_edge = false;
    // Retained slot pins and feedback state, including ACK edges between
    // PH0 transitions. The chip's equation listing is second-sight-reference
    // data/lattice/Lattice_original-BE8C.ldf, lines 424-445.
    slot_bus_transfer_t phase_transfer;
    ss_capture_result_t phase_decode;
    bool phase_high = false;
    Z180Time ready_since;
    void phase(bool high, Z180Time timestamp);
    [[gnu::always_inline]] inline void transfer_rise(slot_bus_transfer_t &transfer, Z180Time now, Z180Time rise, bool &started, bool &high);
    [[gnu::always_inline]] inline bool transfer_fall(slot_bus_transfer_t &transfer, Z180Time sample, uint64_t fall, bool held);
    [[gnu::always_inline]] inline void service_card_deadlines(Z180Time now);
    [[gnu::always_inline]] inline Z180Time limit_card_deadline(Z180Time end);
    uint32_t cpu_hz = EXTAL_HZ / 2;
    // Periods at cpu_hz: one execution tick, one PHI and the copy /WR. Each
    // copy uses them several times; converting each time costs divisions.
    Z180Time tick_period, phi_period, copy_write_period;
    void update_periods();
    double wait_seconds[2] = {-1, -1};
    unsigned wait_phi[2]{};
    uint32_t wait_rate[2]{};
    double event_seconds = -1;
    Z180Time event_time;
    uint64_t transfer_epoch_tick = UINT64_MAX;
    uint64_t transfer_period = 0;
    Z180Time transfer_epoch_time;

    // Absolute board time = host reset epoch + elapsed processor time.
    Z180Time card_time() const { return host_epoch + cpu->time(); }
    // Same time in seconds for the video model, whose deadlines use doubles.
    double cpu_time() const { return card_time().as_seconds(); }
    // Publish stable ROM/SRAM read pages to the CPU after overlay/address changes.
    // Pages selecting a device retain callbacks so their side effects are not skipped.
    void update_direct_reads();
    // Run the U17 copy controller edges due at or before now: BUSREQ, the
    // SRAM write from the live latches, then the release. Every pin edge
    // checks, so keep the idle test inline.
    void advance_copy(Z180Time now) {
        if (copy_deadline <= now)
            run_copy_edges(now);
    }
    void run_copy_edges(Z180Time now);
    // Record one rising CATCH at when; the U5/U6 address latches already hold
    // its address.
    // now is the current card time, which the PHI edge search needs.
    void copy_capture(Z180Time when, Z180Time now);
    // BUSACK while the U17 copy controller requests the bus: /WR starts at
    // the next PHI edge.
    void copy_grant(Z180Time when, Z180Time now);
    // The next copy controller edge the CPU must stop at; never while the
    // CPU itself holds that edge as a scheduled board edge.
    Z180Time copy_wake() const {
        if (copy_phase == CopyPhase::idle || copy_phase == CopyPhase::busack || cpu->board_edge_scheduled())
            return Z180Time::never;
        return copy_deadline;
    }
    // The first rising PHI edge after when; now is the current card time,
    // and when is at most one PHI before it.
    Z180Time next_phi_edge(Z180Time when, Z180Time now) const;
    // Try to schedule the copy controller's BUSREQ release inside the current
    // CPU grant; return false when the scheduler must service it instead.
    bool schedule_copy_release(Z180Time now);
    // Have the CPU call back at copy controller edge at, inside its grant.
    // False leaves the edge to copy_wake().
    bool schedule_copy_edge(Z180Time at, Z180Time now);
    // Convert a video event in absolute seconds to integer card time, rounding
    // forward. Cache unchanged deadlines to avoid repeated floating-point conversion.
    Z180Time vga_deadline(double event);
    // Advance Oak to card time. Dispatch an event whose integer deadline has
    // passed even when the floating-point card time rounds below it.
    void advance_vga();
    // Convert absolute host ticks to card time using a retained whole-second epoch.
    Z180Time transfer_deadline(uint64_t ticks);
    // True if execution passed this host input by at least one half-PHI.
    // A smaller difference is the rounding needed to reach a CPU clock edge.
    bool input_passed(uint64_t ticks);
    // Accept a new processor PHI rate hz and invalidate cached wait conversions.
    void clock_changed(uint32_t hz);
    // Advance CPU and video up to an absolute card deadline; awaiting_ack stops
    // once the selected host acknowledgement permits the Apple to continue.
    void run_to(Z180Time deadline, bool awaiting_ack = false);
    // Run to this host access and apply it synchronously; return extra Apple
    // RDY wait ticks and update transfer.data/driven for a card-driven read.
    uint64_t process(slot_bus_transfer_t &transfer);
    // Decode and accept transfer at absolute card timestamp; return host wait
    // ticks and retain any event or SRAM copy until its hardware hand-off completes.
    uint64_t accept_transfer(slot_bus_transfer_t &transfer, Z180Time timestamp);
    // Process queue in order and run through absolute host ticks. Queued
    // accesses cannot return an unreported RDY delay; such a delay is an invariant failure.
    uint64_t run_transfers(std::vector<slot_bus_transfer_t> &queue, uint64_t ticks);
    // Submit queued accesses through host ticks. If wait is true, return only
    // after completion; access is an optional final synchronous access. Return its
    // RDY wait ticks. Caller storage must survive the submitted job.
    uint64_t dispatch(uint64_t ticks, bool wait, slot_bus_transfer_t *access = nullptr);
    // Post one worker entry: a plain transfer, or a time with an optional
    // final access whose RDY ticks are written to held. Host thread only.
    void post(slot_bus_transfer_t const &transfer, slot_bus_transfer_t *access, uint64_t *held, uint64_t ticks, bool plain,
              bool conditional = false);
    // Host thread, after the card has completed every posted entry: rethrow
    // its error and collect its Apple IRQ edges for later timestamped delivery.
    void finish_synchronize();
    // Move worker IRQ edges into host storage under their own lock. This does
    // not stop Oak or invoke irq_handler; deliver_irq performs that step.
    void collect_irq();
    // Worker loop: retain the active instruction stack across bus deadlines,
    // result waits and presentation pauses until reset or shutdown.
    void stream_entries();
    // Cancel a parked handler at its exact tick before applying reset.
    void prepare_reset();
    // Wait for the worker to complete count entries.
    void wait_for(uint64_t count);
    // Record an asserted/deasserted Apple interrupt request on the worker.
    // Host synchronization or presentation collects edges in order; delivery
    // can be later than the edge's emulated time.
    void publish_irq(bool asserted);
    // Translate the Z180 video window address plus board half/linear selections
    // into an Oak address. This changes address wiring, not memory contents.
    uint32_t oak_address(uint32_t address) const;
    // Present the retained event to Z180 INT1 and DREQ1 until firmware acknowledges it.
    void drive_event();
    // Apply low-three-bit board port selection with data on the card bus.
    // The IORQ select edge can acknowledge an event before the data sample.
    void strobe(unsigned port, uint8_t data);
    // Return full PHI wait clocks for physical address/access, combining the
    // board access delay with current CPU clock conversion (UM0050 pp. 28-30).
    unsigned extra_wait(Z180::Access access, uint32_t address);
};
