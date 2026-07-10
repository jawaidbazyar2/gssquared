/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Mutable system definition for the config editor. Same shape as SystemConfig
 *   load results (SystemConfig_t + disk_mount_t[]), with owned strings.
 */

#pragma once

#include <string>
#include <vector>

#include "../systemconfig.hpp"
#include "util/mount.hpp"

class SystemConfig;

/**
 * Derive the drive list that would be registered at power-on for this
 * platform + slot map, without instantiating devices.
 */
std::vector<drive_spec_t> derive_drives_from_config(PlatformId_t platform_id,
                                                    const device_id slot_devices[NUM_SLOTS]);

/** Basename for display on drive buttons. */
std::string storage_display_name(const std::string& path);

class ConfigDraft {
    std::string name_;
    std::string description_;
    std::string path_;
    std::string id_;
    SystemConfig_t config_{};
    std::vector<disk_mount_t> mounts_;

    void sync_pointers();

public:
    ConfigDraft();

    /** Empty/minimal draft for the given platform (no cards, no storage). */
    void reset_for_platform(PlatformId_t platform_id);

    /** Copy from a loaded SystemConfig (Open and Edit). */
    void load_from(const SystemConfig& config);

    /** Copy from a builtin SystemConfig_t (optional starting point). */
    void load_from_builtin(const SystemConfig_t& config);

    const SystemConfig_t& config() const { return config_; }
    SystemConfig_t& config() { return config_; }
    const std::vector<disk_mount_t>& mounts() const { return mounts_; }
    std::vector<disk_mount_t>& mounts() { return mounts_; }
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const std::string& path() const { return path_; }
    const std::string& id() const { return id_; }

    void set_name(const std::string& name);
    void set_description(const std::string& description);
    void set_path(const std::string& path);
    void set_id(const std::string& id);

    void set_platform(PlatformId_t platform_id);
    void set_slot_device(int slot, device_id id);
    void clear_mounts_for_slot(int slot);
    void set_mount(uint16_t slot, uint16_t drive, const std::string& filename);
    void clear_mount(uint16_t slot, uint16_t drive);

    /** Drive specs with synthetic status from draft mounts. */
    std::vector<drive_spec_t> drive_specs() const;

    /** Slot display name for UI (device human name or empty). */
    std::string slot_device_name(int slot) const;
};
