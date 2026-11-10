/*
 *   Connect the Second Sight card model to GSSquared's Apple IIgs machine.
 *   The card's Z8S180 runs its firmware in SSCard; this adapter supplies the
 *   ROM, observes Apple bus accesses, saves configuration memory and attaches
 *   the monitor. All callbacks here run on the application's main thread.
 *
 *   Mega II is the IIgs chip supplying the slow Apple II memory and slot bus.
 *   An IRQ is an interrupt request to the Apple CPU. EEPROM is electrically
 *   erasable memory that keeps the card's settings after power-off.
 *   Sources: Second Sight manual (installation and monitor choice), Apple
 *   IIgs Technical Note 68 (Mega II bank signal), and ROM_BEEF.asm Reset at
 *   https://github.com/jawaidbazyar2/secondsight . Resource loading, file
 *   persistence and callback lifetime are application policy.
 */

#include "ss_z180.hpp"
#include "ss_card.hpp"
#include "ss_display.hpp"
#include "computer.hpp"
#include "cpu.hpp"
#include "mmus/mmu_iigs.hpp"
#include "videosystem.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/ResourceFile.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "paths.hpp"
#include "util/SystemSettings.hpp"

// Lifetime bundle owned by the machine's shutdown hook. It owns SSCard and
// its display; the saved EEPROM snapshot detects changes on exit. Only main-
// thread callbacks access this bundle, and card accessors drain worker jobs.
struct secondsight_z180_state_t {
    std::unique_ptr<SSCard> card;
    std::unique_ptr<SecondSightDisplay> display;
    bool rgb_monitor = false;
    int firmware = SystemSettings::SS_FW_14;
    bool lattice_v2 = false;
    bool int0_pullup = false;
    std::string eeprom_path;
    std::array<uint16_t, 64> eeprom_initial;
    // Card for the clock's Mega II cycle hook; cleared before the card goes.
    std::shared_ptr<std::atomic<SSCard *>> time_target;
};

// Restore the serial EEPROM words without clocking its serial interface.
static void secondsight_load_eeprom(secondsight_z180_state_t *st) {
    std::ifstream file(st->eeprom_path, std::ios::binary);
    std::array<uint8_t, 128> data;
    // Accept exactly 128 bytes. A missing, short or oversized file leaves the
    // erased chip defaults intact. Disk words are low byte first.
    if (file.read((char *)data.data(), data.size()) && file.peek() == std::ifstream::traits_type::eof()) {
        std::array<uint16_t, 64> words;
        for (size_t i = 0; i < words.size(); ++i)
            words[i] = data[i * 2] | (uint16_t(data[i * 2 + 1]) << 8);
        st->card->video().serial_eeprom().set_contents(words);
    }
    st->eeprom_initial = st->card->video().serial_eeprom().contents();
}

// Persist changed EEPROM contents; ordinary card reset retains these words.
static void secondsight_save_eeprom(secondsight_z180_state_t *st) {
    const auto &words = st->card->video().serial_eeprom().contents();
    if (words == st->eeprom_initial)
        return;
    std::array<uint8_t, 128> data;
    for (size_t i = 0; i < words.size(); ++i) {
        data[i * 2] = words[i];
        data[i * 2 + 1] = words[i] >> 8;
    }
    // Write a complete replacement before renaming it over the saved file.
    // A write failure must not truncate the last usable EEPROM image.
    const std::string temporary = st->eeprom_path + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file.write((const char *)data.data(), data.size());
    file.close();
    std::error_code error;
    if (file)
        std::filesystem::rename(temporary, st->eeprom_path, error);
    if (!file || error)
        fprintf(stderr, "Second Sight: failed to save EEPROM %s\n", st->eeprom_path.c_str());
}

// Catch st up to computer's host clock and allocate a debugger panel snapshot.
// The caller owns the returned formatter; no live worker references escape.
static DebugFormatter *secondsight_z180_debug(secondsight_z180_state_t *st, computer_t *computer) {
    st->card->catch_up(computer->clock->get_c14m());
    auto *df = new DebugFormatter;
    auto &card = *st->card;
    const auto state = card.processor().get_state();
    const auto &capture = card.capture_state();
    df->addLine("Second Sight (with Z180)");
    df->addLine("Z180 PC: %04X physical: %05X clock: %u T states: %llu", state.pc,
                card.processor().physical_address(state.pc), card.processor().clock_hz(), state.tstates);
    df->addLine("Apple data in: %02X out: %02X event: %02X pending: %d bands: %02X", capture.host_input, card.output_latch(),
                capture.event_address, capture.pending, capture.bands);
    df->addLine("D3: %d inhibit: %d B: %d HIRES: %d PAGE2: %d RAMWRT: %d 80STORE: %d", capture.mode_d3,
                capture.copy_inhibit, capture.bank_held, capture.hires, capture.page2, capture.ramwrt, capture.store80);
    df->addLine("Copies: %llu events: %llu stalls: %llu held 14M ticks: %llu", card.copies, card.events, card.stalls,
                card.stall_ticks);
    return df;
}

// Load the selected 128 KiB EPROM through the normal resource path; reject
// a truncated image before connecting it to the processor bus.
static std::unique_ptr<uint8_t[]> secondsight_load_rom(const char *filename) {
    const std::string path = std::string("roms/cards/secondsight/") + filename;
    ResourceFile rom(path.c_str(), READ_ONLY);
    std::unique_ptr<uint8_t[]> image(rom.load());
    if (rom.size() != SSCard::ROM_SIZE)
        throw std::runtime_error("Second Sight: " + path + " must be 131072 bytes");
    return image;
}

// Install the card in computer at slot, which identifies its Apple IRQ source.
// The shutdown callback owns the state allocated here. Firmware selection is
// independent of the U7 Lattice chip capture equations.
void init_secondsight_z180(computer_t *computer, SlotType_t slot) {
    auto *mmu = dynamic_cast<MMU_IIgs *>(computer->cpu->mmu);
    if (!mmu)
        throw std::runtime_error("Second Sight (with Z180) requires an Apple IIgs");
    const auto &settings = SystemSettings::instance();
    auto image = secondsight_load_rom(settings.ss_rom_filename());
    auto *st = new secondsight_z180_state_t;
    st->firmware = settings.ss_firmware();
    st->lattice_v2 = settings.ss_lattice_v2();
    st->int0_pullup = settings.ss_int0_pullup();
    st->card = std::make_unique<SSCard>(image.get(), computer->clock->get_c14m_per_second(),
                                      st->lattice_v2 ? SSCapture::LatticeMap::V2 : SSCapture::LatticeMap::BE8C,
                                      SSCard::Presentation::Latest, st->int0_pullup);
    st->rgb_monitor = SystemSettings::instance().ss_rgb_monitor();
    st->card->video().monitor_type_w(st->rgb_monitor);
    if (computer->get_config_path().empty()) Paths::calc_pref(st->eeprom_path, "secondsight.eeprom");
    else st->eeprom_path = computer->get_config_path() + ".secondsight.eeprom";
    secondsight_load_eeprom(st);
    // The card worker records IRQ edges; this callback runs on the host
    // when they are delivered. Relaxed synchronization can make that delivery
    // later than the card-edge timestamp; the IIgs CPU is not rewound.
    st->card->irq_handler = [computer, slot](bool asserted) {
        computer->irq_control->set_irq((device_irq_id)slot, asserted);
    };
    // RDY progress advances motherboard devices on the host thread while
    // the 65816 retains its current access. It does not deliver card IRQs.
    st->card->held_progress = [mmu](uint64_t ticks) { mmu->advance_megaii_hold(ticks); };
    st->card->host_clock_ticks = [clock = computer->clock] { return clock->get_c14m(); };
    st->card->start_worker();
    // Every Mega II access before the current cycle has been observed, so
    // the card thread may run up to this time without waiting for a request.
    st->time_target = std::make_shared<std::atomic<SSCard *>>(st->card.get());
    computer->clock->set_cycle_handler([target = st->time_target, clock = computer->clock] {
        if (SSCard *card = target->load(std::memory_order_relaxed))
            card->publish_time(clock->get_c14m());
    });

    // M2B0 is the original Apple bank bit, needed to distinguish main and
    // auxiliary video writes. ROM 01 exposes it only in slot 3 (Technical
    // Note 68); ROM 03 in slots 1-6. Slot 7's pin 35 carries CREF instead
    // (Hardware Reference p. 176; Second Sight manual p. 30: slots 1-6).
    mmu->set_megaii_observer([st, mmu, slot, clock = computer->clock](megaii_transfer_t &transfer) -> uint64_t {
        if (mmu->is_rom03_machine() ? slot == SLOT_7 : slot != SLOT_3)
            transfer.m2b0 = false;
        // A debugger access is not a bus cycle; it may only read the port.
        if (transfer.inspect) {
            st->card->inspect(transfer, clock->get_c14m());
            return 0;
        }
        return st->card->observe(transfer);
    });

    // Card instruction-boundary grants end before the next full-state observer.
    // A frame that can stop early (debugger, calibration) has no known fence.
    computer->register_begin_frame_handler([st, clock = computer->clock](bool to_frame_end) {
        st->card->set_observation_limit(to_frame_end ? clock->get_frame_end_c14M() : 0);
    });

    // Offer the current card scan while the Apple picture is drawn. Inspection
    // and stepping retain exact fences. This is also where a monitor choice
    // becomes a cable-sense input. The firmware samples it when setting a
    // mode, so it cannot rescale an active mode immediately (ROM_BEEF.asm
    // monitor/mode initialization).
    computer->device_frame_dispatcher->registerHandler([st, computer]() {
        // SecondSightDisplay claims stopped Oak state or reuses its previous
        // texture. Stepping may skip rendering and therefore synchronizes here.
        st->card->request_presentation(computer->clock->get_c14m(), computer->execution_mode != EXEC_NORMAL);
        const bool rgb = SystemSettings::instance().ss_rgb_monitor();
        if (st->rgb_monitor != rgb) {
            // A changed cable sense is an input at this host time, including
            // paused/stepping frames whose presentation may be skipped.
            st->card->catch_up(computer->clock->get_c14m());
            st->rgb_monitor = rgb;
            st->card->video().monitor_type_w(rgb);
            if (st->display)
                st->display->show_notice("Second Sight monitor changed; if blank, change mode or Restart");
        }
        return true;
    });
    st->display = std::make_unique<SecondSightDisplay>(computer, *st->card);
    // Replacing ROM or capture equations, or fitting the INT0 pull-up, requires
    // a card power cycle. An ordinary Apple reset preserves the U7 Lattice chip
    // and captured RAM.
    computer->register_reset_handler([st, computer](bool cold_start) {
        const auto &settings = SystemSettings::instance();
        if (cold_start && (st->firmware != settings.ss_firmware() || st->lattice_v2 != settings.ss_lattice_v2() ||
                           st->int0_pullup != settings.ss_int0_pullup())) {
            auto image = secondsight_load_rom(settings.ss_rom_filename());
            st->card->change_hardware(computer->clock->get_c14m(), image.get(),
                                     settings.ss_lattice_v2() ? SSCapture::LatticeMap::V2 : SSCapture::LatticeMap::BE8C,
                                     settings.ss_int0_pullup());
            st->firmware = settings.ss_firmware();
            st->lattice_v2 = settings.ss_lattice_v2();
            st->int0_pullup = settings.ss_int0_pullup();
        } else {
            // With unchanged hardware, Apple restart does not power off U7 Lattice chip.
            st->card->reset(computer->clock->get_c14m());
        }
        st->rgb_monitor = SystemSettings::instance().ss_rgb_monitor();
        st->card->video().monitor_type_w(st->rgb_monitor);
        return true;
    });
    // Stop new bus submissions first, finish work, detach the display, then
    // save EEPROM while the chip still exists. Deleting st joins its worker.
    computer->register_shutdown_handler([st, computer, mmu]() {
        // Detach both host inputs before destroying the card. The clock
        // retains its callback, so its shared target must become null.
        mmu->set_megaii_observer({});
        st->time_target->store(nullptr, std::memory_order_relaxed);
        st->card->catch_up(computer->clock->get_c14m());
        st->display.reset();
        secondsight_save_eeprom(st);
        delete st;
        return true;
    });
    computer->register_debug_display_handler("ss-z180", DH_SECOND_SIGHT_Z180,
                                             [st, computer]() { return secondsight_z180_debug(st, computer); });
}
