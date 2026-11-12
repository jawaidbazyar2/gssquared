/*
 * A current LCD monitor on the card's VGA output. Its controller measures the
 * sync, picks one of its stored modes, samples that mode's part of each line
 * and scales the samples to the panel. The monitor's rules and numbers here are
 * read from the firmware of a monitor with a Realtek RTD2556 controller and a
 * 1920 x 1080 panel (Keil 8051 code, version RT2556.51_D18213); bN:XXXX names a
 * routine or table in its bank N. The register meanings come from the Realtek
 * RTD2660 data sheet: it has the same sync processor and capture registers.
 * What the user does with the monitor's controls (the screen Auto runs on, the
 * 640/720 width choice, Clock, Phase and position) is this model's choice, and
 * each such comment says so.
 */
#pragma once

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ss_lcd {

// A stored mode. flags: the sync polarities it is made for (bit 0 HN VN, 1 HP
// VN, 2 HN VP, 3 HP VP), interlace (bit 6) and video inputs (bits 4, 5, 7).
// Rates are in 0.1 kHz and 0.1 Hz. samples is the ADC clock in samples per
// line; hstart and vstart put the capture in samples after the HSync leading
// edge and in lines after the VSync leading edge.
struct Preset {
    uint8_t flags;
    uint16_t width, height, line_rate, field_rate;
    uint8_t line_tolerance, field_tolerance;
    uint16_t samples, lines, hstart, vstart;
};
// Two presets that one timing can match, and the sync width up to which the
// first one is right: HSync in 1/27 us, VSync in lines.
struct Pair {
    uint8_t first, second;
    uint16_t limit;
};

// The presets in the order of the firmware's table, which the search keeps.
inline constexpr Preset presets[149] = {
    {0x02, 640, 350, 315, 700, 10, 10, 800, 449, 144, 61},
    {0x02, 640, 350, 315, 700, 10, 10, 800, 449, 144, 61},
    {0x09, 640, 400, 248, 564, 10, 10, 848, 440, 145, 32},
    {0x04, 640, 400, 315, 600, 10, 10, 800, 525, 144, 42},
    {0x04, 720, 400, 315, 700, 10, 10, 900, 449, 166, 36},
    {0x04, 640, 400, 315, 700, 10, 10, 800, 449, 145, 36},
    {0x04, 720, 400, 380, 850, 10, 10, 936, 446, 181, 44},
    {0x04, 640, 400, 380, 850, 10, 10, 832, 446, 160, 44},
    {0x01, 640, 480, 246, 496, 10, 10, 800, 497, 144, 14},
    {0x05, 640, 480, 315, 600, 10, 10, 800, 525, 147, 34},
    {0x07, 640, 480, 350, 667, 10, 10, 864, 525, 160, 41},
    {0x05, 640, 480, 378, 728, 10, 10, 832, 520, 171, 30},
    {0x05, 640, 480, 375, 750, 10, 10, 840, 500, 187, 18},
    {0x0f, 640, 480, 433, 850, 10, 10, 832, 510, 137, 27},
    {0x0f, 720, 480, 298, 599, 10, 10, 896, 497, 160, 16},
    {0x01, 720, 480, 315, 600, 10, 10, 856, 525, 124, 36},
    {0x0f, 720, 576, 358, 600, 10, 10, 912, 600, 169, 22},
    {0x07, 720, 576, 455, 756, 10, 10, 944, 602, 142, 22},
    {0x08, 800, 600, 351, 562, 10, 10, 1024, 625, 204, 23},
    {0x08, 800, 600, 378, 603, 10, 10, 1056, 628, 220, 26},
    {0x0f, 800, 600, 440, 700, 10, 10, 1040, 624, 190, 29},
    {0x08, 800, 600, 481, 722, 10, 10, 1040, 666, 188, 28},
    {0x08, 800, 600, 468, 750, 10, 10, 1056, 625, 244, 23},
    {0x0e, 800, 600, 536, 850, 10, 10, 1048, 631, 217, 29},
    {0x01, 832, 624, 497, 745, 10, 10, 1152, 667, 288, 41},
    {0x0e, 848, 480, 310, 600, 10, 10, 1088, 517, 224, 31},
    {0x0f, 848, 480, 350, 700, 10, 10, 1072, 500, 200, 18},
    {0x0f, 848, 480, 360, 720, 10, 10, 1090, 501, 207, 20},
    {0x0e, 848, 480, 376, 750, 10, 10, 1090, 504, 200, 21},
    {0x0f, 1024, 768, 396, 500, 10, 10, 1312, 793, 248, 25},
    {0x01, 1024, 768, 483, 600, 10, 10, 1344, 806, 296, 34},
    {0x09, 1024, 768, 483, 600, 10, 10, 1320, 813, 218, 6},
    {0x01, 1024, 768, 540, 661, 10, 10, 1328, 816, 288, 39},
    {0x01, 1024, 768, 565, 700, 10, 10, 1328, 806, 280, 34},
    {0x0f, 1024, 768, 577, 720, 10, 10, 1360, 801, 280, 31},
    {0x08, 1024, 768, 600, 750, 10, 10, 1312, 800, 272, 30},
    {0x01, 1024, 768, 601, 750, 10, 10, 1328, 804, 272, 32},
    {0x08, 1024, 768, 682, 850, 10, 10, 1376, 808, 304, 38},
    {0x0d, 1152, 864, 537, 600, 10, 10, 1512, 894, 300, 30},
    {0x0e, 1152, 864, 642, 702, 10, 10, 1480, 912, 296, 46},
    {0x08, 1152, 864, 675, 750, 10, 10, 1600, 900, 384, 34},
    {0x0b, 1152, 864, 770, 850, 10, 10, 1576, 907, 360, 42},
    {0x01, 1152, 870, 687, 750, 10, 10, 1456, 915, 272, 41},
    {0x08, 1152, 900, 618, 660, 10, 10, 1504, 937, 323, 35},
    {0x03, 1152, 900, 618, 660, 10, 10, 1504, 937, 320, 34},
    {0x0c, 1280, 720, 450, 600, 10, 10, 1650, 750, 303, 23},
    {0x0f, 1280, 720, 525, 700, 10, 10, 1683, 750, 339, 29},
    {0x0f, 1280, 720, 540, 720, 10, 10, 1700, 751, 344, 30},
    {0x0f, 1280, 720, 564, 750, 10, 10, 1696, 755, 336, 32},
    {0x0f, 1280, 768, 396, 499, 10, 10, 1648, 793, 312, 21},
    {0x06, 1280, 768, 478, 599, 10, 10, 1664, 798, 320, 26},
    {0x02, 1280, 768, 474, 600, 10, 10, 1440, 790, 112, 18},
    {0x04, 1280, 768, 560, 700, 10, 10, 1692, 799, 336, 31},
    {0x0e, 1280, 768, 603, 749, 10, 10, 1696, 805, 336, 33},
    {0x04, 1280, 768, 686, 848, 10, 10, 1712, 809, 351, 37},
    {0x0f, 1280, 800, 497, 598, 10, 10, 1680, 831, 328, 26},
    {0x0f, 1280, 800, 491, 590, 13, 15, 1440, 823, 112, 20},
    {0x0f, 1280, 800, 584, 699, 10, 10, 1696, 833, 344, 32},
    {0x0f, 1280, 800, 602, 719, 10, 10, 1712, 834, 352, 33},
    {0x04, 1280, 800, 627, 748, 10, 10, 1696, 838, 328, 34},
    {0x08, 1280, 960, 600, 600, 10, 10, 1800, 1000, 424, 38},
    {0x0f, 1280, 960, 699, 700, 10, 10, 1727, 999, 359, 38},
    {0x0f, 1280, 960, 699, 720, 10, 10, 1700, 990, 359, 40},
    {0x08, 1280, 960, 746, 744, 10, 10, 1728, 1002, 360, 38},
    {0x0f, 1280, 960, 860, 850, 10, 10, 1728, 1011, 384, 50},
    {0x08, 1280, 1024, 640, 600, 10, 10, 1688, 1066, 360, 40},
    {0x04, 1280, 1024, 746, 700, 10, 10, 1696, 1072, 384, 33},
    {0x01, 1280, 1024, 746, 700, 10, 10, 1678, 1064, 328, 26},
    {0x0e, 1280, 1024, 770, 720, 10, 10, 1728, 1082, 360, 44},
    {0x0a, 1280, 1024, 800, 750, 10, 10, 1688, 1066, 394, 40},
    {0x0f, 1280, 1024, 911, 850, 10, 10, 1728, 1072, 384, 47},
    {0x0c, 1360, 768, 477, 600, 10, 10, 1792, 795, 368, 24},
    {0x08, 1366, 768, 477, 600, 10, 10, 1792, 798, 356, 27},
    {0x0d, 1440, 900, 559, 598, 10, 10, 1904, 934, 385, 30},
    {0x02, 1440, 900, 555, 599, 10, 10, 1600, 926, 112, 22},
    {0x0f, 1440, 900, 706, 745, 10, 10, 1936, 942, 398, 38},
    {0x09, 1600, 1200, 750, 600, 10, 10, 2160, 1250, 496, 49},
    {0x0f, 1600, 1200, 740, 599, 10, 10, 1760, 1235, 112, 31},
    {0x0f, 1600, 1200, 812, 650, 10, 10, 2160, 1250, 496, 49},
    {0x0f, 1600, 1200, 875, 700, 10, 10, 2160, 1250, 496, 49},
    {0x0d, 1680, 1050, 653, 600, 10, 10, 2240, 1089, 456, 35},
    {0x0a, 1680, 1050, 647, 600, 10, 10, 1840, 1080, 112, 26},
    {0x04, 1680, 1050, 828, 753, 10, 10, 2272, 1099, 472, 46},
    {0x05, 1920, 1200, 746, 600, 10, 10, 2592, 1245, 536, 42},
    {0x02, 1920, 1200, 740, 600, 10, 10, 2080, 1235, 112, 32},
    {0x0d, 1920, 1440, 900, 600, 10, 10, 2600, 1500, 552, 59},
    {0x0d, 720, 576, 312, 500, 10, 10, 864, 625, 135, 42},
    {0x4d, 1440, 240, 157, 600, 10, 10, 1716, 262, 238, 20},
    {0x4f, 1920, 540, 281, 500, 10, 10, 2640, 562, 242, 21},
    {0x4f, 1920, 540, 337, 600, 10, 10, 2200, 562, 242, 21},
    {0x0d, 1920, 1080, 562, 500, 10, 10, 2640, 1125, 239, 39},
    {0x04, 1920, 1080, 672, 600, 10, 10, 2576, 1120, 528, 37},
    {0x0f, 1152, 900, 717, 760, 10, 10, 1472, 943, 304, 40},
    {0x04, 1600, 1000, 622, 599, 10, 10, 2128, 1038, 432, 35},
    {0x0f, 1600, 1000, 616, 599, 10, 10, 1760, 1029, 112, 26},
    {0x0f, 1600, 1000, 784, 748, 10, 10, 2160, 1047, 448, 44},
    {0x4d, 1440, 288, 157, 500, 10, 10, 1728, 312, 264, 24},
    {0x02, 1920, 1080, 675, 600, 10, 10, 2200, 1125, 192, 41},
    {0x0a, 1920, 1080, 666, 600, 10, 10, 2080, 1111, 112, 28},
    {0x04, 1176, 664, 417, 604, 10, 10, 1512, 690, 288, 23},
    {0x04, 1176, 664, 500, 725, 25, 30, 1512, 690, 288, 23},
    {0x01, 640, 500, 310, 577, 10, 10, 808, 541, 148, 12},
    {0x04, 848, 480, 298, 597, 10, 10, 1056, 500, 184, 17},
    {0x02, 1600, 900, 555, 599, 10, 10, 1760, 926, 115, 22},
    {0x08, 1680, 1050, 815, 749, 10, 10, 1840, 1088, 112, 35},
    {0x01, 1280, 1024, 811, 761, 10, 10, 1664, 1066, 352, 40},
    {0x05, 640, 500, 315, 580, 10, 10, 808, 537, 144, 13},
    {0x08, 1600, 900, 600, 600, 10, 10, 1800, 1000, 176, 99},
    {0x0a, 2048, 1152, 720, 600, 10, 10, 2250, 1200, 176, 47},
    {0x04, 1280, 720, 448, 599, 10, 10, 1664, 748, 320, 24},
    {0x04, 1920, 1080, 556, 499, 10, 10, 2544, 1114, 512, 31},
    {0x02, 1920, 1200, 614, 500, 10, 10, 2080, 1229, 112, 26},
    {0x04, 2048, 1152, 716, 599, 10, 10, 2752, 1195, 568, 40},
    {0x08, 1920, 1080, 675, 600, 10, 10, 2200, 1125, 239, 39},
    {0x02, 640, 350, 379, 850, 10, 10, 832, 445, 160, 61},
    {0x02, 1920, 1440, 888, 600, 10, 10, 2080, 1481, 112, 38},
    {0x02, 2048, 1152, 710, 599, 10, 10, 2208, 1185, 112, 30},
    {0x04, 1152, 864, 770, 850, 10, 10, 1552, 910, 320, 43},
    {0x04, 1600, 1280, 795, 599, 10, 10, 2160, 1327, 448, 44},
    {0x0f, 720, 480, 315, 600, 10, 10, 858, 525, 127, 34},
    {0x0f, 1280, 720, 375, 500, 10, 10, 1980, 750, 303, 23},
    {0x04, 1600, 1000, 612, 585, 10, 10, 2160, 1047, 448, 34},
    {0x4f, 1920, 540, 313, 500, 10, 10, 2304, 625, 352, 80},
    {0x0f, 1600, 900, 559, 599, 10, 10, 2112, 934, 424, 31},
    {0x0a, 1400, 1050, 647, 600, 10, 10, 1560, 1080, 112, 27},
    {0x0a, 1400, 1050, 653, 600, 10, 10, 1864, 1089, 376, 36},
    {0x08, 1600, 1200, 938, 750, 10, 10, 2160, 1250, 496, 49},
    {0x02, 2048, 1536, 947, 600, 10, 10, 2208, 1580, 112, 41},
    {0x01, 1024, 768, 601, 749, 10, 10, 1360, 802, 280, 33},
    {0x04, 1400, 1050, 822, 748, 10, 10, 1896, 1099, 392, 46},
    {0x04, 1280, 800, 716, 849, 10, 10, 1712, 843, 352, 40},
    {0x04, 1440, 900, 804, 848, 10, 10, 1952, 948, 408, 45},
    {0x04, 1400, 1050, 939, 850, 10, 10, 1912, 1105, 408, 52},
    {0x08, 1600, 1200, 1063, 850, 10, 10, 2160, 1250, 496, 49},
    {0x04, 1680, 1050, 939, 849, 10, 10, 2288, 1105, 480, 52},
    {0x04, 1920, 1200, 1072, 849, 10, 10, 2624, 1262, 560, 59},
    {0x28, 1920, 1080, 270, 240, 10, 10, 2750, 1125, 292, 41},
    {0x28, 1920, 1080, 281, 250, 10, 10, 2640, 1125, 292, 41},
    {0x6d, 1440, 288, 157, 500, 10, 10, 1728, 312, 264, 24},
    {0x6d, 1440, 240, 157, 600, 10, 10, 1716, 262, 238, 20},
    {0x6f, 1920, 540, 281, 500, 10, 10, 2640, 562, 242, 21},
    {0x6f, 1920, 540, 313, 500, 10, 10, 2304, 625, 352, 80},
    {0x6f, 1920, 540, 337, 600, 10, 10, 2200, 562, 242, 21},
    {0x1f, 720, 480, 315, 600, 10, 10, 858, 525, 127, 34},
    {0x1d, 720, 576, 312, 500, 10, 10, 864, 625, 135, 42},
    {0x2f, 1280, 720, 375, 500, 10, 10, 1980, 750, 303, 23},
    {0x2c, 1280, 720, 450, 600, 10, 10, 1650, 750, 303, 23},
    {0x2d, 1920, 1080, 562, 500, 10, 10, 2640, 1125, 239, 39},
    {0x28, 1920, 1080, 675, 600, 10, 10, 2200, 1125, 239, 39},
};
// Pairs that the HSync width, the VSync width, the polarity or interlace tell apart.
inline constexpr Pair hsync_pairs[15] = {
    {107, 60, 25}, {14, 25, 65}, {51, 50, 30}, {51, 71, 25}, {103, 73, 25}, {77, 76, 21}, {77, 83, 18},
    {84, 83, 17}, {81, 80, 20}, {124, 125, 21}, {98, 91, 20}, {60, 107, 20}, {71, 50, 40}, {74, 73, 25},
    {74, 123, 15},
};
inline constexpr Pair vsync_pairs[25] = {
    {125, 80, 5}, {124, 81, 5}, {129, 82, 5}, {77, 84, 5}, {37, 54, 5}, {132, 134, 5}, {103, 74, 5},
    {123, 73, 5}, {69, 105, 5}, {9, 119, 4}, {9, 15, 4}, {9, 143, 4}, {36, 53, 5}, {72, 50, 5}, {9, 25, 5},
    {12, 28, 4}, {15, 25, 7}, {72, 30, 4}, {72, 71, 4}, {76, 83, 4}, {76, 84, 4}, {35, 53, 5}, {72, 51, 4},
    {103, 74, 4}, {103, 73, 4},
};
inline constexpr Pair polarity_pairs[9] = {
    {0, 4, 0}, {1, 4, 0}, {0, 5, 0}, {1, 5, 0}, {114, 7, 0}, {114, 6, 0}, {30, 50, 0}, {30, 51, 0},
    {30, 71, 0},
};
inline constexpr Pair interlace_pairs[4] = {
    {86, 122, 0}, {144, 141, 0}, {86, 141, 0}, {144, 122, 0},
};

// The panel shows the capture with the aspect setting 4:3, in 1440 x 1080 of
// its 1920 x 1080: the user picks the setting that keeps the shape of the 4:3
// monitors the card was made for. The factory setting is Full (default
// settings at b3:6B8D, byte 11).
inline constexpr float aspect = 4.0f / 3.0f;
inline constexpr int panel_width = 1440, panel_height = 1080;

// The scaler's filters for enlarging the capture, one for each of the 5
// sharpness settings (b1:BEB8 to b1:C0B8, chosen through the table at
// b1:C138). Coefficients are in 1/1024. Entry k is for a panel pixel
// (k + 0.5) / 32 of a sample after sample n; its 4 rows weigh samples n + 2,
// n + 1, n and n - 1. The second half of the sample mirrors the first.
inline constexpr int16_t scale_up_filters[5][4][16] = {
    {
        {-6, -5, -4, -2, 0, 1, 5, 8, 12, 16, 21, 26, 31, 37, 43, 51},
        {232, 246, 260, 275, 289, 306, 319, 335, 350, 366, 380, 395, 408, 423, 438, 451},
        {580, 579, 577, 573, 569, 563, 557, 550, 541, 532, 523, 512, 502, 490, 478, 464},
        {218, 204, 191, 178, 166, 154, 143, 131, 121, 110, 100, 91, 83, 74, 65, 58},
    },
    {
        {-11, -10, -11, -12, -13, -14, -15, -17, -18, -18, -20, -20, -21, -21, -21, -19},
        {137, 161, 180, 208, 228, 257, 278, 309, 331, 362, 385, 416, 438, 469, 491, 520},
        {777, 772, 768, 758, 751, 737, 726, 709, 695, 674, 658, 634, 616, 591, 570, 542},
        {121, 101, 87, 70, 58, 44, 35, 23, 16, 6, 1, -6, -9, -15, -16, -19},
    },
    {
        {-32, -23, -24, -23, -24, -25, -28, -29, -31, -33, -36, -37, -40, -41, -43, -43},
        {76, 97, 120, 146, 172, 201, 230, 262, 294, 328, 361, 397, 431, 468, 502, 539},
        {922, 907, 900, 885, 872, 854, 836, 813, 789, 762, 736, 704, 675, 641, 609, 572},
        {58, 43, 28, 16, 4, -6, -14, -22, -28, -33, -37, -40, -42, -44, -44, -44},
    },
    {
        {-10, -10, -11, -14, -16, -20, -24, -28, -32, -38, -44, -50, -57, -62, -68, -74},
        {9, 34, 60, 90, 120, 155, 189, 228, 264, 306, 347, 391, 433, 478, 521, 568},
        {1036, 1029, 1021, 1006, 990, 968, 945, 915, 886, 852, 817, 778, 740, 696, 655, 609},
        {-11, -29, -46, -58, -70, -79, -86, -91, -94, -96, -96, -95, -92, -88, -84, -79},
    },
    {
        {-5, -2, -3, -4, -5, -6, -9, -11, -15, -19, -23, -28, -34, -39, -45, -51},
        {-57, -37, -16, 12, 39, 73, 107, 147, 188, 234, 278, 329, 378, 432, 483, 540},
        {1156, 1144, 1132, 1111, 1090, 1059, 1028, 988, 948, 902, 857, 806, 756, 701, 649, 592},
        {-70, -81, -89, -95, -100, -102, -102, -100, -97, -93, -88, -83, -76, -70, -63, -57},
    },
};
// The factory sharpness (default settings at b3:6B8D, byte 9 bits 5-3). A
// VGA input uses the same filter across and down (b2:EA3E).
inline constexpr int factory_sharpness = 2;

// The controller counts its 14.318 MHz crystal, but converts the counts as
// 14318 kHz.
inline constexpr double crystal_hz = 14'318'180.0;
inline constexpr unsigned crystal_khz = 14318;

// The sync as the controller measures it: the line period in crystal clocks,
// the line and field rates in 0.1 kHz and 0.1 Hz, the lines of a field and the
// active widths of the sync pulses in crystal clocks and lines.
struct Timing {
    int period = 0, line_rate = 0, field_rate = 0, lines = 0, hsync = 0, vsync = 0;
    bool h_positive = false, v_positive = false, interlaced = false;
    bool operator==(const Timing &o) const {
        return period == o.period && line_rate == o.line_rate && field_rate == o.field_rate && lines == o.lines &&
            hsync == o.hsync && vsync == o.vsync && h_positive == o.h_positive && v_positive == o.v_positive &&
            interlaced == o.interlaced;
    }
};

// Measure a signal. False when the controller finds no signal: it needs a
// line period that its 11-bit stable period counter holds, and a VSync within
// each 60 ms wait.
inline bool measure(double line_hz, int lines, double hsync_seconds, int vsync_lines, bool h_positive,
                    bool v_positive, bool interlaced, Timing &timing) {
    if (!(line_hz > 0) || lines < 1 || line_hz / lines < 1 / 0.060) return false;
    const long period = std::lround(crystal_hz / line_hz);
    if (period < 2 || period > 2048) return false;
    timing.period = int(period);
    timing.line_rate = int((crystal_khz * 20 / unsigned(period) + 1) / 2);
    timing.field_rate = int((crystal_khz * 20000u / unsigned(period) / unsigned(lines) + 1) / 2);
    timing.lines = lines;
    timing.hsync = int(std::lround(hsync_seconds * crystal_hz));
    timing.vsync = vsync_lines;
    timing.h_positive = h_positive;
    timing.v_positive = v_positive;
    timing.interlaced = interlaced;
    return true;
}

// The most samples per line that the 210 MHz ADC takes at this line rate, a
// multiple of 4. This count uses the period register without its +1.
inline int max_samples(const Timing &timing) {
    const unsigned rate = (crystal_khz * 20 / unsigned(timing.period - 1) + 1) / 2;
    return rate ? std::min(int(2100000 / rate / 4 * 4), 3200) : 3200;
}

// Lines that are left for the picture: the total less the lines in 550 us
// (the VESA CVT minimum for VSync and back porch) and a front porch of 3.
inline uint16_t active_estimate(int lines, int field_rate) {
    return uint16_t(lines - int(uint32_t(lines) * uint32_t(field_rate) * 11 / 200000) - 3);
}
inline int distance(int a, int b) {
    return std::abs(int(int16_t(uint16_t(a - b))));
}

// The stored mode for a timing, or -1 for none.
inline int search(const Timing &t) {
    if (t.line_rate < 100 || t.line_rate > 2200 || t.field_rate < 230 || t.field_rate > 1500) return -1;
    constexpr int count = int(std::size(presets));
    const int limit = max_samples(t);
    const int polarity = t.h_positive ? (t.v_positive ? 8 : 2) : (t.v_positive ? 4 : 1);
    const uint16_t estimate = active_estimate(t.lines, t.field_rate);
    auto line_fits = [&](int i) { return distance(t.line_rate, presets[i].line_rate) <= presets[i].line_tolerance; };
    auto field_fits = [&](int i) {
        return distance(t.field_rate, presets[i].field_rate) <= presets[i].field_tolerance;
    };
    auto valid = [&](int i, bool exact) {
        const auto &p = presets[i];
        return (exact || p.samples <= limit) && t.lines > p.height && p.samples > p.width + p.hstart;
    };
    auto paired = [](const Pair &pair, int a, int b) {
        return (pair.first == a || pair.second == a) && (pair.first == b || pair.second == b);
    };
    auto tiebreak = [&](int i, int best, bool exact) {
        if (!exact) {
            // A preset whose own line rate, then field rate, fits the signal.
            if (line_fits(best) != line_fits(i)) return line_fits(best) ? best : i;
            if (field_fits(best) != field_fits(i)) return field_fits(best) ? best : i;
            // Else the height nearer the estimate, unless both presets have one timing.
            const bool same = distance(presets[best].line_rate, presets[i].line_rate) <= 10 &&
                distance(presets[best].field_rate, presets[i].field_rate) <= 10 &&
                distance(presets[best].lines, presets[i].lines) <= 2;
            if (!same) {
                const int a = distance(presets[i].height, estimate), b = distance(presets[best].height, estimate);
                if (!(a < 3 && b < 3) && a != b) return a < b ? i : best;
            }
        }
        for (const auto &pair : hsync_pairs)
            if (paired(pair, best, i))
                return t.hsync <= int(pair.limit * crystal_khz / 27000) ? int(pair.first) : int(pair.second);
        for (const auto &pair : vsync_pairs)
            if (paired(pair, best, i)) return t.vsync <= pair.limit ? int(pair.first) : int(pair.second);
        if (!((presets[best].flags & 0x40) && (presets[i].flags & 0x40)))
            for (const auto &pair : interlace_pairs)
                if (paired(pair, best, i)) {
                    if (bool(presets[pair.first].flags & 0x40) == t.interlaced) return int(pair.first);
                    if (bool(presets[pair.second].flags & 0x40) == t.interlaced) return int(pair.second);
                }
        const int pi = presets[i].flags & 15, pb = presets[best].flags & 15;
        if (pi != pb && (pi & polarity) != (pb & polarity))
            for (const auto &pair : polarity_pairs)
                if (paired(pair, best, i)) {
                    if (presets[pair.first].flags & polarity) return int(pair.first);
                    if (presets[pair.second].flags & polarity) return int(pair.second);
                }
        // The card's VGA output has separate syncs, so the presets for video
        // inputs lose; then the wider preset wins, and the earlier on a tie.
        const bool video_best = presets[best].flags & 0xb0, video_i = presets[i].flags & 0xb0;
        if (video_best != video_i) return video_best ? i : best;
        return presets[i].width > presets[best].width ? i : best;
    };
    auto pass = [&](bool exact) {
        int best = -1;
        for (int i = 0; i < count; ++i) {
            const auto &p = presets[i];
            const bool match = exact
                ? field_fits(i) && line_fits(i) && distance(t.lines, p.lines) <= 2
                : distance(active_estimate(p.lines, p.field_rate), estimate) <= 20;
            if (match && valid(i, exact)) best = best < 0 ? i : tiebreak(i, best, exact);
        }
        return best;
    };
    int best = pass(true);
    if (best >= 0 && presets[best].samples > limit) return -1;
    if (best < 0) best = pass(false);
    if (best < 0) return -1;
    // 640 x 480 at 60 Hz becomes 720 x 480 when its VSync is longer than 4
    // lines, and 400 lines at 70 or 85 Hz with HSync positive and VSync
    // negative become 640 x 350.
    if (best == 9 || best == 15 || best == 119)
        best = t.vsync <= 4 ? 9 : (t.h_positive || t.v_positive) ? 119 : 15;
    if ((best == 4 || best == 5) && t.h_positive && !t.v_positive) best = 0;
    if ((best == 6 || best == 7) && t.h_positive && !t.v_positive) best = 114;
    return best;
}

// The controller supports a timing in these limits, and shows "no support"
// for others.
inline bool supports(const Timing &t, int index, int samples) {
    if (index < 0) return false;
    const auto &p = presets[index];
    if (t.line_rate < 100 || t.line_rate > 2200 || t.field_rate < 230 || t.field_rate > 1500) return false;
    if (p.height < 240 || p.width > 4096) return false;
    return uint32_t(samples) * uint32_t(t.line_rate) / 10 / 100 < 2100;
}

// The screen that the user runs Auto on, one field, in pixels of the card
// from the start of its active display, with the sync leading edges in the
// same counts: width pixels a line, lines a field. level is the brightest
// color of each pixel, which the boundary search compares with the noise
// margin; blue is the color that the difference sum reads. edge is the rise
// and fall time of the video, in pixels: a sample in it reads a level on the
// way from the pixel before.
struct Picture {
    int width = 0, lines = 0, hsync = 0, vsync = 0;
    std::vector<uint8_t> level, blue;
    double edge = 0;
    // The active display: pixels and lines of the field from its start.
    int active_width = 0, active_lines = 0;
};
// Samples after the HSync or lines after the VSync leading edge.
struct Window {
    int first = 0, last = -1;
};
// The bright part of the picture, in samples after the HSync and lines after
// the VSync leading edge, as the capture start counts them.
struct Box {
    bool lit = false;
    int left = 0, right = 0, top = 0, bottom = 0;
    int width() const { return right - left + 1; }
    int height() const { return bottom - top + 1; }
};

// The noise margin of the boundary search and the 64 steps of the ADC phase
// in a sample (RTD2660 registers 0x76-0x78 and 0x7C).
inline constexpr int noise_margin = 0x40, phases = 64;

// One line of a plane of the picture, counted from the VSync leading edge.
inline const uint8_t *picture_line(const Picture &picture, const std::vector<uint8_t> &plane, int line) {
    return plane.data() + size_t(((picture.vsync + line + 1) % picture.lines + picture.lines) % picture.lines) *
        size_t(picture.width);
}
// The level of sample s at an ADC clock of samples per line. Phase p takes it
// p/64 of a sample after the start of its period. No source gives where phase
// 0 falls against the video; here it falls on the HSync leading edge.
inline int sample_level(const Picture &picture, const uint8_t *line, int samples, int s, int phase) {
    const int64_t at = (int64_t(phases) * s + phase) * picture.width, period = int64_t(phases) * samples;
    const int x = int((picture.hsync + at / period) % picture.width);
    const int before = (x + picture.width - 1) % picture.width;
    const double into = double(at % period) / double(period);
    if (into < picture.edge) return int(line[before] + (line[x] - line[before]) * into / picture.edge);
    return line[x];
}

// The auto function's boundary search. It starts after the HSync pulse and
// the clamp pulse (its end at the RTD2660 reset value, 16), stops just before
// the next HSync, and searches from the line after the VSync pulse to the
// second last line.
inline Box boundary(const Picture &picture, const Timing &t, int samples, int phase) {
    Box box;
    if (picture.width < 1 || picture.lines < 1 || samples < 1) return box;
    const int pixel_clock = int(uint32_t(samples) * uint32_t(t.line_rate) / 1000);
    const int first = 16 + 5 + int(int64_t(t.hsync) * samples / t.period);
    const int last = samples - (pixel_clock * (t.h_positive ? 80 : 100) / 10000 + 1);
    for (int y = t.vsync + 1; y <= t.lines - 2; ++y) {
        const uint8_t *line = picture_line(picture, picture.level, y);
        for (int s = first; s <= last; ++s) {
            if (sample_level(picture, line, samples, s, phase) <= noise_margin) continue;
            if (!box.lit) { box.lit = true; box.left = box.right = s; box.top = box.bottom = y; }
            box.left = std::min(box.left, s);
            box.right = std::max(box.right, s);
            box.bottom = y;
            break;
        }
        for (int s = last; s > box.right && box.lit; --s)
            if (sample_level(picture, line, samples, s, phase) > noise_margin) { box.right = s; break; }
    }
    return box;
}

// Where the active display falls, in samples after the HSync and lines after
// the VSync leading edge.
inline Box active_box(const Picture &picture, int samples, int phase) {
    Box box;
    for (int s = 0; s < samples; ++s) {
        const int64_t at = (int64_t(phases) * s + phase) * picture.width, period = int64_t(phases) * samples;
        if ((picture.hsync + at / period) % picture.width >= picture.active_width) continue;
        if (!box.lit) box.left = s;
        box.lit = true;
        box.right = s;
    }
    bool lines = false;
    for (int y = 0; y < picture.lines; ++y) {
        if ((picture.vsync + y + 1) % picture.lines >= picture.active_lines) continue;
        if (!lines) box.top = y;
        lines = true;
        box.bottom = y;
    }
    box.lit = box.lit && lines;
    return box;
}

// The auto function's difference sum, in the edge mode on blue as the
// firmware sets it (RTD2660 registers 0x79-0x7D): over the window, each blue
// difference between neighbour samples that is over the threshold. A line
// the same as the one before has the same sum.
inline uint32_t difference_sum(const Picture &picture, int samples, int phase, int threshold, Window h, Window v) {
    uint32_t total = 0, sum = 0;
    const uint8_t *previous = nullptr;
    const int first = std::max(1, h.first), last = std::min(samples - 1, h.last);
    for (int y = v.first; y <= v.last; ++y) {
        const uint8_t *line = picture_line(picture, picture.blue, y);
        if (!previous || std::memcmp(line, previous, size_t(picture.width))) {
            sum = 0;
            int before = sample_level(picture, line, samples, first - 1, phase);
            for (int s = first; s <= last; ++s) {
                const int now = sample_level(picture, line, samples, s, phase);
                if (std::abs(now - before) > threshold) sum += uint32_t(std::abs(now - before));
                before = now;
            }
        }
        previous = line;
        total += sum;
    }
    return total;
}

// The capture of a frame: the preset, the ADC clock and phase, and the window
// that the panel shows, in samples after the HSync and lines after the VSync
// leading edge.
struct Capture {
    int index = -1;
    int samples = 0, hstart = 0, width = 0, vstart = 0, height = 0, phase = 0;
};

// The controller. For the best picture the user presses Auto for each new
// mode and each new shape of its picture, with the whole picture on the
// screen, and then centers the picture in the capture: a smaller picture gets
// even bands, a bigger one loses the same at both sides. The monitor itself
// runs Auto only for presets wider than 720.
class Monitor {
public:
    // Follow the signal of one frame. False when there is no signal.
    bool follow(bool signal, const Timing &timing) {
        if (!signal) {
            signal_ = false;
            capture_ = {};
            return false;
        }
        if (!signal_ || !(timing == timing_)) detect(timing);
        signal_ = true;
        return true;
    }
    bool supported() const { return signal_ && supports(timing_, index_, samples_); }
    const Capture &capture() const { return capture_; }
    // A new mode needs Auto.
    bool needs_picture() const { return supported() && pending_; }
    // Auto, from the preset's values. 400-line modes at 70 and 85 Hz have the
    // same timing 640 or 720 samples wide; Realtek's RTD2662 firmware lets the
    // user pick the width (GET_MODESELECT_TYPE, Mode.c:447-466). The user who
    // sets the best picture picks the width whose samples fall on the card's
    // pixels in the shorter repeating pattern, 720 on a tie.
    void picture(const Picture &picture) {
        if (!supported()) return;
        pending_ = false;
        adjust(picture, index_);
        const int other = index_ == 4 ? 5 : index_ == 6 ? 7 : -1;
        if (other < 0 || picture.width < 1 || !supports(timing_, other, presets[other].samples)) {
            set_capture();
            return;
        }
        const int index = index_, samples = samples_, hstart = hstart_, vstart = vstart_, phase = phase_;
        adjust(picture, other);
        if (samples_ / std::gcd(samples_, picture.width) >= samples / std::gcd(samples, picture.width)) {
            index_ = index, samples_ = samples, hstart_ = hstart, vstart_ = vstart, phase_ = phase;
        }
        set_capture();
    }

private:
    void detect(const Timing &timing) {
        timing_ = timing;
        index_ = search(timing);
        if (index_ < 0) {
            capture_ = {};
            pending_ = false;
            return;
        }
        const auto &p = presets[index_];
        samples_ = p.samples;
        hstart_ = p.hstart;
        vstart_ = p.vstart;
        phase_ = 0;
        pending_ = true;
        set_capture();
    }
    // The capture start register, which the clock must leave room after.
    static int start_register(int hstart) { return (hstart <= 983 ? 10 : hstart - 973) + 2; }
    void set_capture() {
        const auto &p = presets[index_];
        capture_ = {index_, samples_, hstart_, p.width, vstart_, p.height, phase_};
        // A 350-line mode shows as a 400-line mode with 25 lines above and
        // below, and its capture starts 4 lines after VSync at the earliest.
        if (p.width == 640 && p.height == 350) {
            int start = std::max(vstart_, 4 + 25);
            if (start + 350 + 3 > timing_.lines) start = std::max(0, timing_.lines - 353);
            capture_.vstart = start - 25;
            capture_.height = 400;
        }
    }
    // Auto for one preset, from its table values.
    void adjust(const Picture &picture, int index) {
        index_ = index;
        const auto &p = presets[index];
        samples_ = p.samples;
        hstart_ = p.hstart;
        vstart_ = p.vstart;
        phase_ = 0;
        auto_adjust(picture);
    }
    // Auto (firmware b4:ECBA) sets the clock and the phase. Then the user sets
    // Clock and Phase by hand (tune) and centers the picture with the position
    // controls; the monitor's own position step would leave a picture that
    // fits the capture where it is. When an Auto step fails, the rest does
    // not change.
    void auto_adjust(const Picture &picture) {
        const auto &p = presets[index_];
        Box box = boundary(picture, timing_, samples_, phase_);
        if (!box.lit) return;
        // The clock changes only when the picture fills at least 95% of the
        // preset's part of the line (b4:E648).
        const int before = samples_;
        if (uint32_t(box.width()) * p.samples >= uint32_t(p.width) * 95 * uint32_t(before) / 100) {
            int samples = samples_;
            if (!coarse_clock(picture, samples, box)) return;
            if (!fine_clock(picture, samples, before, box)) return;
            samples_ = samples;
        }
        auto_phase(picture, box);
        tune(picture);
        box = boundary(picture, timing_, samples_, phase_);
        if (!box.lit) return;
        // The user crops border that does not fit evenly, but keeps the whole
        // active display when it fits. The capture starts at least 1 sample
        // after the start register and 3 lines after the first line that the
        // controller can start on.
        const Box active = active_box(picture, samples_, phase_);
        int left = box.left - (p.width - box.width()) / 2, top = box.top - (p.height - box.height()) / 2;
        if (active.lit && active.width() <= p.width) left = std::clamp(left, active.right - p.width + 1, active.left);
        if (active.lit && active.height() <= p.height) top = std::clamp(top, active.bottom - p.height + 1, active.top);
        const int first = std::max(4, p.height * 6 / 1080 + 2) + 3;
        hstart_ = std::max(start_register(0) + 1, left);
        const int last = std::max(first, timing_.lines - p.height);
        vstart_ = std::clamp(top, first, last);
    }
    // Scale the clock until the bright part is as wide as the preset (b4:D180).
    bool coarse_clock(const Picture &picture, int &samples, Box &box) const {
        const int width = presets[index_].width, limit = max_samples(timing_);
        int previous = 0;
        for (int n = 0; n < 10; ++n) {
            if (samples < start_register(hstart_) + width || samples > limit) return false;
            box = boundary(picture, timing_, samples, phase_);
            if (!box.lit) return false;
            const int w = box.width();
            if (w == width || previous + w == 2 * width) return true;
            previous = w;
            const int next = int((int64_t(samples) * width + w / 2) / w);
            if (next == samples) return true;
            samples = next;
        }
        return false;
    }
    // The hardware phase sweep (b4:B10A): the difference sums of 16 phases 4
    // apart. Plain: the largest and smallest sum, and the first phase with
    // the largest. Weighted: around each phase t1 - |t1 - t0| - |t1 - t2|,
    // around the circle, and the first phase with the largest above 0.
    struct Sweep {
        uint32_t high = 0, low = 0xffffffff;
        int phase = 0;
    };
    static uint32_t apart(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }
    Sweep sweep(const Picture &picture, int samples, int threshold, bool weighted) {
        threshold_ = threshold;
        uint32_t sums[16];
        for (int i = 0; i < 16; ++i)
            sums[i] = difference_sum(picture, samples, 4 * i, threshold, hwindow_, vwindow_);
        Sweep result;
        for (int n = weighted ? 1 : 0; n < (weighted ? 17 : 16); ++n) {
            const int i = n % 16;
            if (!weighted) {
                if (sums[i] > result.high) { result.high = sums[i]; result.phase = 4 * i; }
                result.low = std::min(result.low, sums[i]);
                continue;
            }
            const uint32_t centre = sums[i], off = apart(centre, sums[(i + 15) % 16]) + apart(centre, sums[(i + 1) % 16]);
            if (centre > off && centre - off > result.high) { result.high = centre - off; result.phase = 4 * i; }
        }
        return result;
    }
    // An odd clock moves to the even neighbour on a multiple of 4.
    static int even_clock(int clock) {
        if (!(clock & 1)) return clock;
        return ((clock + 1) & 3) ? clock - 1 : clock + 1;
    }
    // The fine step (b4:A391) tries the even clocks near the coarse one inside
    // the bright part, and keeps the one whose sum changes most with the
    // phase: there each sample falls at the same place in its pixel. Without
    // a clear winner the coarse clock stays, made even.
    bool fine_clock(const Picture &picture, int &samples, int before, Box &box) {
        const auto &p = presets[index_];
        const uint32_t area = uint32_t(p.width) * p.height;
        const int coarse = samples;
        hwindow_ = {box.left, box.right};
        vwindow_ = {box.top, box.bottom};
        int low = coarse - (2 * before / p.width + 1), high = coarse + before / p.width + 2;
        low -= low & 1;
        high += high & 1;
        uint32_t best_change = 0, ratio = 0;
        int best = coarse;
        for (int clock = low; clock <= high; clock += 2) {
            const Sweep s = sweep(picture, clock, 0x40, false);
            if (s.high - s.low > best_change) {
                best = clock;
                best_change = s.high - s.low;
                ratio = s.low ? s.high / s.low : s.high > area * 64 / 128 ? 0xffffffff : 0;
            }
        }
        if (best_change <= area * 64 / 256) {
            samples = even_clock(coarse);
            return true;
        }
        if (ratio >= 5) {
            samples = best;
            return true;
        }
        // Check the winner again with a higher threshold at its best phase.
        const Sweep s = sweep(picture, best, 0xa0, false);
        box = boundary(picture, timing_, best, s.phase);
        if (!box.lit) return false;
        const bool fits = box.width() == p.width;
        if (s.high)
            ratio = s.low ? s.high / s.low : s.high > area * 0xa0 / 512 ? 0xffffffff : 0;
        samples = (s.high && ratio >= 5) || fits ? best : even_clock(coarse);
        return true;
    }
    // After Auto the user turns Clock and Phase until no bars move through the
    // picture, as monitor guides advise for vertical lines (Dell P2423 guide:
    // "Adjust the Phase and Pixel Clock controls via OSD"). Clock: the value
    // whose samples fall on the card's pixels in the shortest repeating
    // pattern, inside the OSD range around Auto's clock (b2:A4AB), with the
    // active display no wider than before in the capture; the nearest such
    // value wins a tie. Phase: the one that keeps every sample farthest from
    // a pixel edge.
    void tune(const Picture &picture) {
        const auto &p = presets[index_];
        const int pixels = picture.width;
        if (pixels < 1) return;
        const int range = std::min({50, max_samples(timing_) - samples_, samples_ - (start_register(hstart_) + p.width)});
        const int64_t limit = std::max(int64_t(p.width) * pixels, int64_t(picture.active_width) * samples_);
        auto repeat = [pixels](int clock) { return clock / std::gcd(clock, pixels); };
        int best = samples_;
        for (int clock = samples_ - range; clock <= samples_ + range; ++clock) {
            if (clock < 1 || int64_t(picture.active_width) * clock > limit) continue;
            const int r = repeat(clock), b = repeat(best);
            if (r < b || (r == b && std::abs(clock - samples_) < std::abs(best - samples_))) best = clock;
        }
        samples_ = best;
        // Distances in 1/(64 x samples) of a card pixel, over one repeat.
        const int64_t period = int64_t(phases) * best;
        int64_t margin = -1;
        int phase = phase_;
        for (int n = 0; n < phases; ++n) {
            const int candidate = (phase_ + n) % phases;
            int64_t least = period;
            for (int k = 0; k < repeat(best) && least > margin; ++k) {
                const int64_t at = (int64_t(phases) * k + candidate) * pixels % period;
                least = std::min(least, std::min(at, period - at));
            }
            if (least > margin) { margin = least; phase = candidate; }
        }
        phase_ = phase;
    }
    // The phase (b4:AE2A): a weighted sweep inside the bright part, then the
    // sums of 5 phases 2 apart around its result, weighted the same way. The
    // phase stays when no weighted sum is above 0.
    void auto_phase(const Picture &picture, const Box &box) {
        if (box.width() > 4 && box.height() > 4) {
            hwindow_ = {box.left + 2, box.right - 2};
            vwindow_ = {box.top + 2, box.bottom - 2};
        }
        Sweep s = sweep(picture, samples_, 0x80, true);
        if (s.high / 128 < 100) s = sweep(picture, samples_, 0x40, true);
        uint32_t t0 = 0, t1 = 0, t2 = 0, top = 0;
        int found = phase_;
        for (int i = 0; i < 5; ++i) {
            const int phase = (s.phase - 4 + 2 * i) & (phases - 1);
            t0 = t1;
            t1 = t2;
            t2 = difference_sum(picture, samples_, phase, threshold_, hwindow_, vwindow_);
            const uint32_t off = apart(t1, t0) + apart(t1, t2);
            if (t1 > off && t1 - off > top) { top = t1 - off; found = (phase - 2) & (phases - 1); }
        }
        phase_ = found;
    }
    Timing timing_;
    bool signal_ = false, pending_ = false;
    int index_ = -1, samples_ = 0, hstart_ = 0, vstart_ = 0, phase_ = 0, threshold_ = 0;
    Window hwindow_, vwindow_;
    Capture capture_;
};

// The panel picture as the emulator shows it. The scaler enlarges the
// capture to the panel's 4:3 area, across and then down, each panel pixel from
// the 4 samples around its position. A window smaller than the panel shows,
// in each of its pixels, the average of the panel pixels it covers. No source
// gives the scaler's start phase; here sample and panel pixel centers line up
// at both ends.
class Panel {
public:
    // Set up for a capture of width x height samples shown in out_width x
    // out_height pixels, at most the panel's size. True when the taps changed.
    bool configure(int width, int height, int out_width, int out_height, int sharpness = factory_sharpness) {
        out_width = std::clamp(out_width, 1, panel_width);
        out_height = std::clamp(out_height, 1, panel_height);
        if (width == width_ && height == height_ && out_width == out_width_ && out_height == out_height_ &&
            sharpness == sharpness_)
            return false;
        width_ = width;
        height_ = height;
        out_width_ = out_width;
        out_height_ = out_height;
        sharpness_ = sharpness;
        across_ = filter(width, panel_width, out_width, sharpness);
        down_ = filter(height, panel_height, out_height, sharpness);
        rows_.assign(size_t(height) * out_width * 3, 0);
        pixels_.assign(size_t(out_width) * out_height, 0xff000000);
        return true;
    }
    int width() const { return out_width_; }
    int height() const { return out_height_; }
    // The same taps as texels for the GPU filter (ss_lcd_gpu.hpp), which
    // reads 8-bit channels: output column o in texel (o, k) of the first
    // gpu_taps rows, output row o in the same texel of the next gpu_taps rows.
    // Bytes 0 and 1 of a texel hold its weight in 1/4096 plus 32768; bytes 2
    // and 3 the first sample plus 4096 times the number of taps. Fills
    // 2 * gpu_taps rows of width texels. False when a pixel needs more taps.
    static constexpr int gpu_taps = 15;
    bool gpu_tables(uint32_t *rows, int width) const {
        std::fill_n(rows, size_t(2 * gpu_taps) * width, 0u);
        auto put = [width](const std::vector<Taps> &axis, uint32_t *base) {
            if (int(axis.size()) > width) return false;
            for (size_t o = 0; o < axis.size(); ++o) {
                const Taps &t = axis[o];
                if (t.weight.size() > size_t(gpu_taps) || t.first > 4095) return false;
                const uint32_t head = uint32_t(t.first) | uint32_t(t.weight.size()) << 12;
                for (size_t k = 0; k < t.weight.size(); ++k)
                    base[k * size_t(width) + o] = uint32_t(t.weight[k] + 32768) | head << 16;
            }
            return true;
        };
        return put(across_, rows) && put(down_, rows + size_t(gpu_taps) * width);
    }
    // Filter rows top to bottom of a width x height capture of XRGB pixels.
    // Returns the first and last output rows that changed.
    std::pair<int, int> update(const uint32_t *capture, int top, int bottom) {
        top = std::max(top, 0);
        bottom = std::min(bottom, height_ - 1);
        if (bottom < top) return {0, -1};
        for (int y = top; y <= bottom; ++y) {
            const uint32_t *line = capture + size_t(y) * width_;
            int16_t *out = rows_.data() + size_t(y) * out_width_ * 3;
            for (const Taps &t : across_) {
                int sum[3] = {weight_one / 2, weight_one / 2, weight_one / 2};
                for (size_t k = 0; k < t.weight.size(); ++k) {
                    const uint32_t px = line[t.first + int(k)];
                    sum[0] += t.weight[k] * int((px >> 16) & 255);
                    sum[1] += t.weight[k] * int((px >> 8) & 255);
                    sum[2] += t.weight[k] * int(px & 255);
                }
                for (int c = 0; c < 3; ++c) *out++ = int16_t(sum[c] >> weight_bits);
            }
        }
        int first = out_height_, last = -1;
        for (int y = 0; y < out_height_; ++y) {
            const Taps &t = down_[size_t(y)];
            if (t.first + int(t.weight.size()) - 1 < top || t.first > bottom) continue;
            first = std::min(first, y);
            last = y;
            uint32_t *out = pixels_.data() + size_t(y) * out_width_;
            for (int x = 0; x < out_width_ * 3; x += 3) {
                int value[3];
                for (int c = 0; c < 3; ++c) {
                    int sum = weight_one / 2;
                    for (size_t k = 0; k < t.weight.size(); ++k)
                        sum += t.weight[k] * rows_[(size_t(t.first) + k) * out_width_ * 3 + x + c];
                    value[c] = std::clamp(sum >> weight_bits, 0, 255);
                }
                *out++ = 0xff000000u | uint32_t(value[0]) << 16 | uint32_t(value[1]) << 8 | uint32_t(value[2]);
            }
        }
        return {first, last};
    }
    const uint32_t *pixels() const { return pixels_.data(); }

private:
    static constexpr int weight_bits = 12, weight_one = 1 << weight_bits;
    // Consecutive samples from first on and their weights for one output pixel.
    struct Taps {
        int first = 0;
        std::vector<int> weight;
    };
    // Each output pixel covers out_width / panel of the panel's pixels.
    static std::vector<Taps> filter(int samples, int panel, int out, int sharpness) {
        const auto &f = scale_up_filters[sharpness];
        std::vector<Taps> result(static_cast<size_t>(out));
        std::vector<double> sum(static_cast<size_t>(samples));
        for (int o = 0; o < out; ++o) {
            std::fill(sum.begin(), sum.end(), 0.0);
            int low = samples, high = -1;
            // Panel pixel p covers [p * out, (p + 1) * out); this output pixel
            // covers [o * panel, (o + 1) * panel) in the same units.
            for (int p = int(int64_t(o) * panel / out); p < panel && int64_t(p) * out < int64_t(o + 1) * panel; ++p) {
                const int64_t cover = std::min(int64_t(p + 1) * out, int64_t(o + 1) * panel) -
                    std::max(int64_t(p) * out, int64_t(o) * panel);
                if (cover <= 0) continue;
                const double part = double(cover) / panel;
                // The position of panel pixel p in samples, in 1/32 of a sample.
                const int64_t at = (32 * int64_t(p) + 16) * samples / panel - 16;
                const int n = int((at >= 0 ? at : at - 31) / 32), k = int(at - 32 * int64_t(n));
                int w[4];
                if (k < 16) {
                    w[0] = f[3][k], w[1] = f[2][k], w[2] = f[1][k], w[3] = f[0][k];
                } else {
                    const int j = 31 - k;
                    w[0] = f[0][j], w[1] = f[1][j], w[2] = f[2][j], w[3] = f[3][j];
                }
                for (int i = 0; i < 4; ++i) {
                    const int sample = std::clamp(n - 1 + i, 0, samples - 1);
                    sum[size_t(sample)] += part * w[i] / 1024.0;
                    low = std::min(low, sample);
                    high = std::max(high, sample);
                }
            }
            Taps &t = result[size_t(o)];
            t.first = low;
            int total = 0, largest = 0;
            for (int i = low; i <= high; ++i) {
                t.weight.push_back(int(std::lround(sum[size_t(i)] * weight_one)));
                total += t.weight.back();
                if (std::abs(t.weight.back()) > std::abs(t.weight[size_t(largest)])) largest = i - low;
            }
            // Rounding must not change the brightness of a flat area.
            t.weight[size_t(largest)] += weight_one - total;
        }
        return result;
    }
    std::vector<Taps> across_, down_;
    std::vector<int16_t> rows_;
    std::vector<uint32_t> pixels_;
    int width_ = 0, height_ = 0, out_width_ = 0, out_height_ = 0, sharpness_ = -1;
};

} // namespace ss_lcd
