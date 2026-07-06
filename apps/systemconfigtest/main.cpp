/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Load and dump .gs2 system config files; run fixture self-tests with --self-test.
 */

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "paths.hpp"
#include "util/SystemConfig.hpp"

uint64_t debug_level = 0;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--self-test] [-q] <file.gs2>\n";
    std::cerr << "  --self-test  Run built-in fixture tests\n";
    std::cerr << "  -q           Load only; exit 0 on success\n";
}

static std::filesystem::path fixture_dir() {
    static const std::filesystem::path dir =
        std::filesystem::path(__FILE__).parent_path() / "fixtures";
    return dir;
}

static bool load_and_check(const std::filesystem::path& path, bool quiet) {
    SystemConfig config;
    std::string error;
    if (!config.load(path.string(), error)) {
        std::cerr << "FAIL load " << path << ": " << error << "\n";
        return false;
    }
    if (!quiet) {
        config.dump(std::cout);
    }
    return true;
}

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            return false; \
        } \
    } while (0)

static bool test_minimal() {
    const auto path = fixture_dir() / "MinimalIIe.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "MinimalIIe load: " << error);
    CHECK(config.gs2_version() == 1, "gs2_version");
    CHECK(std::string(config.config().name) == "Blank IIe", "name");
    CHECK(config.config().platform_id == PLATFORM_APPLE_IIE_ENHANCED, "platform");
    CHECK(config.config().clock_set == CLOCK_SET_US, "default clock");
    CHECK(config.config().scanner_type == Scanner_AppleIIe, "derived scanner");
    CHECK(config.config().slot_devices[6] == DEVICE_ID_DISK_II, "disk_ii slot 6");
    CHECK(config.mounts().empty(), "no storage");
    return true;
}

static bool test_apple2plus() {
    const auto path = fixture_dir() / "Apple2Plus.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "Apple2Plus load: " << error);
    CHECK(config.config().platform_id == PLATFORM_APPLE_II_PLUS, "platform");
    CHECK(config.config().slot_devices[0] == DEVICE_ID_LANGUAGE_CARD, "lang card");
    CHECK(config.config().slot_devices[5] == DEVICE_ID_PD_BLOCK3, "bazfast3");
    CHECK(config.mounts().size() == 3, "three storage rows");
    CHECK(config.mounts()[0].drive == 0, "drive 1-based to 0-based");
    CHECK(config.mounts()[0].filename.find("ProDOS_32MB.po") != std::string::npos,
          "relative image path resolved");
    return true;
}

static bool test_platforms() {
    struct case_t {
        const char* file;
        PlatformId_t platform;
    };
    static const case_t cases[] = {
        {"platform_apple2.gs2", PLATFORM_APPLE_II},
        {"platform_apple2plus.gs2", PLATFORM_APPLE_II_PLUS},
        {"platform_apple2e.gs2", PLATFORM_APPLE_IIE},
        {"platform_apple2e_enhanced.gs2", PLATFORM_APPLE_IIE_ENHANCED},
        {"platform_apple2e_65816.gs2", PLATFORM_APPLE_IIE_65816},
        {"platform_apple2gs.gs2", PLATFORM_APPLE_IIGS},
    };
    for (const auto& c : cases) {
        SystemConfig config;
        std::string error;
        const auto path = fixture_dir() / c.file;
        CHECK(config.load(path.string(), error), c.file << ": " << error);
        CHECK(config.config().platform_id == c.platform, c.file << " platform id");
    }
    return true;
}

static bool test_clock_scanner() {
    const auto path = fixture_dir() / "IIe_PAL.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "IIe_PAL load: " << error);
    CHECK(config.config().clock_set == CLOCK_SET_PAL, "pal clock");
    CHECK(config.config().scanner_type == Scanner_AppleIIePAL, "pal scanner");
    return true;
}

static bool test_aliases() {
    const auto path = fixture_dir() / "aliases.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "aliases load: " << error);
    CHECK(config.config().slot_devices[5] == DEVICE_ID_PD_BLOCK3, "smartport alias");
    CHECK(config.config().slot_devices[7] == DEVICE_ID_PD_BLOCK2, "pdblock2 alias");
    return true;
}

static bool test_dual_mockingboard() {
    const auto path = fixture_dir() / "DualMock.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "DualMock load: " << error);
    CHECK(config.config().slot_devices[4] == DEVICE_ID_MOCKINGBOARD, "mb slot 4");
    CHECK(config.config().slot_devices[7] == DEVICE_ID_MOCKINGBOARD, "mb slot 7");
    return true;
}

static bool test_storage_multivolume() {
    const auto path = fixture_dir() / "BazFastVolumes.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "BazFastVolumes load: " << error);
    CHECK(config.mounts().size() == 3, "three volumes");
    CHECK(config.mounts()[1].drive == 1, "drive 2 -> index 1");
    return true;
}

static bool test_parallel_output() {
    const auto path = fixture_dir() / "ParallelOutput.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "ParallelOutput load: " << error);
    CHECK(config.card_extras().size() == 1, "one card extra");
    CHECK(config.card_extras()[0].parallel_output == "printouts/session.txt", "parallel path");
    return true;
}

static bool test_connections() {
    const auto path = fixture_dir() / "MyGS.gs2";
    SystemConfig config;
    std::string error;
    CHECK(config.load(path.string(), error), "MyGS load: " << error);
    CHECK(config.connections().size() == 2, "two connections");
    CHECK(!config.connections()[0].slot.has_value(), "builtin SCC A");
    CHECK(config.connections()[0].port == "a", "port a");
    CHECK(config.connections()[1].port == "b", "port b");
    return true;
}

static bool test_load_fails(const char* file, const char* expect_substr) {
    SystemConfig config;
    std::string error;
    const auto path = fixture_dir() / file;
    if (config.load(path.string(), error)) {
        std::cerr << "FAIL: expected load failure for " << file << "\n";
        return false;
    }
    if (std::string(error).find(expect_substr) == std::string::npos) {
        std::cerr << "FAIL: " << file << " error expected to contain '" << expect_substr
                  << "', got: " << error << "\n";
        return false;
    }
    return true;
}

static bool test_errors() {
    CHECK(test_load_fails("bad_version.gs2", "gs2_version"), "bad version");
    CHECK(test_load_fails("dup_slot.gs2", "Duplicate"), "duplicate slot");
    CHECK(test_load_fails("videx_on_gs.gs2", "not allowed"), "videx on gs");
    CHECK(test_load_fails("pal_on_gs.gs2", "pal"), "pal on gs");
    CHECK(test_load_fails("dup_storage.gs2", "Duplicate storage"), "dup storage");
    return true;
}

static bool run_self_tests() {
    struct test_fn {
        const char* name;
        bool (*fn)();
    };
    static const test_fn tests[] = {
        {"minimal", test_minimal},
        {"apple2plus", test_apple2plus},
        {"platforms", test_platforms},
        {"clock_scanner", test_clock_scanner},
        {"aliases", test_aliases},
        {"dual_mockingboard", test_dual_mockingboard},
        {"storage_multivolume", test_storage_multivolume},
        {"parallel_output", test_parallel_output},
        {"connections", test_connections},
        {"errors", test_errors},
    };

    int passed = 0;
    for (const auto& test : tests) {
        std::cout << "Running " << test.name << "...\n";
        if (test.fn()) {
            std::cout << "  PASS\n";
            passed++;
        } else {
            std::cout << "  FAIL\n";
        }
    }
    std::cout << passed << "/" << (int)(sizeof(tests) / sizeof(tests[0])) << " tests passed\n";
    return passed == (int)(sizeof(tests) / sizeof(tests[0]));
}

int main(int argc, char* argv[]) {
    Paths::initialize(true);

    if (argc >= 2 && std::string(argv[1]) == "--self-test") {
        return run_self_tests() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    bool quiet = false;
    int argi = 1;
    if (argc >= 2 && std::string(argv[1]) == "-q") {
        quiet = true;
        argi = 2;
    }

    if (argi >= argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string path = argv[argi];
    SystemConfig config;
    std::string error;
    if (!config.load(path, error)) {
        std::cerr << error << "\n";
        return EXIT_FAILURE;
    }

    if (!quiet) {
        config.dump(std::cout);
    }
    return EXIT_SUCCESS;
}
