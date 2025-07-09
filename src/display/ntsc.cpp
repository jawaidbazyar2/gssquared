/*
 *   Copyright (c) 2025 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _USE_MATH_DEFINES // for C++ M_PI
// https://devblogs.microsoft.com/cppblog/standards-version-switches-in-the-compiler/
//  Properties - C/C++ - Launguage - C++ Language Standard - ISO C++20 Standard (/std:c++20)

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
//#include <chrono>
#include <stdio.h>
#include "display.hpp"
#include "Matrix3x3.hpp"
#include "display/types.hpp"
#include "display/ntsc.hpp"
#include "devices/displaypp/RGBA.hpp"

ntsc_config config ;

// Function to generate phase information for a scanline and stuff in config.
void generatePhaseInfo(int scanlineY, float colorBurst) {
    //ph_info phaseInfo;
    
    // Normalize the color burst phase (similar to the original code)
    float c = colorBurst / (2 * M_PI);
    config.phaseInfo[0] = c - std::floor(c);
    
    // Set phase alternation (in original code this is a boolean array, 
    // but we simplify to alternate based on scanline)
    config.phaseInfo[1] = 0.0f; //(scanlineY % 2) ? 1.0f : -0.0f;

}

// The core pixel function - convert an input luminance to YIQ-like representation
inline void processPixel(uint8_t input, float phase, float phaseAlternation, float* output) {
    // Normalize input byte (0-255 luma) to 0-1 luma.
    float luminance = input / 255.0f;
#if 0    
    // Apply YIQ encoding
    output[0] = luminance;                                    // Y component
    output[1] = luminance * std::sin(phase);                  // I component
    output[2] = luminance * (1.0f - 2.0f * phaseAlternation) * std::cos(phase); // Q component
#else
    output[0] = luminance;
    output[1] = luminance * std::sin(phase);
    output[2] = luminance * std::cos(phase);
#endif
}

// Function to convert YIQ to RGB using a decoding matrix
inline void yiqToRgb(const float yiq[3], /* const Matrix3x3& matrix,  */ /* const float offset[3], */ RGBA_t &rgba) {
    float rgb[3];
    
    // Apply matrix and offset (matrix is 3x3, stored in row-major format)
    rgb[0] = config.decoderMatrix[0] * yiq[0] + config.decoderMatrix[1] * yiq[1] + config.decoderMatrix[2] * yiq[2] /* + offset[0] */;
    rgb[1] = config.decoderMatrix[3] * yiq[0] + config.decoderMatrix[4] * yiq[1] + config.decoderMatrix[5] * yiq[2] /* + offset[1] */;
    rgb[2] = config.decoderMatrix[6] * yiq[0] + config.decoderMatrix[7] * yiq[1] + config.decoderMatrix[8] * yiq[2] /* + offset[2] */;
    
    // Clamp values and convert to 8-bit
    rgba.r = std::clamp(static_cast<int>(rgb[0] * 255), 0, 255);
    rgba.g = std::clamp(static_cast<int>(rgb[1] * 255), 0, 255);
    rgba.b = std::clamp(static_cast<int>(rgb[2] * 255), 0, 255);
    rgba.a = 255;
}

void setupConfig() {
    // TODO: analyze and document - in apple ii land the colorburst is defined as negative 33 degrees,
    // But that shifts the colorburst the wrong way. We needed to bring it counterclockwise around the 
    // colorwheel.
    //float colorBurst = 2.0 * M_PI * (-33.0 / 360.0 + (imageLeft % 4) / 4.0);
    float colorBurst = (2.0 * M_PI * (33.0 / 360.0 )); // plus 0.25, 0.5 0.75, if imageLeft % 4 is 1 2 3
    float subcarrier = NTSC_FSC / NTSC_4FSC; /* 0.25, basically constant here */
    printf("ColorBurst: %.6f\n", colorBurst);
    printf("Subcarrier: %.6f\n", subcarrier);

    //config.phaseInfo = generatePhaseInfo(/* scanlineY */ 0, colorBurst);
    printf("Phase Info:\n");
    printf("Phase[0]: %.6f\n", config.phaseInfo[0]);
    printf("Phase[1]: %.6f\n", config.phaseInfo[1]);

    config.width = FRAME_WIDTH;
    config.height = FRAME_HEIGHT;
    config.colorBurst = colorBurst;
    config.subcarrier = subcarrier;
    config.videoSaturation = 1.0f;
    config.videoHue = 0.05f;
    config.videoBrightness = 0.0f;

   /* config = {
        .width = FRAME_WIDTH,
        .height = FRAME_HEIGHT,
        .colorBurst = colorBurst,
        .subcarrier = subcarrier,
        .videoSaturation = 1.0f,
        .videoHue = 0.05f,
        .videoBrightness = 0.0f,
            // Set up filter coefficients based on measured YIQ filter response
        .filterCoefficients = {
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f }
        },
        .decoderOffset = {0.0f, 0.0f, 0.0f}, // TODO: first value should brightness. -1 to +1.
        .videoSaturation = 1.0f,
        .videoHue = 0.05f,
        .videoBrightness = 0.0f
    }; */
    generatePhaseInfo(/* scanlineY */ 0, colorBurst);

    // pre-calculate the sin/cos values for each of the 4 pixel positions (phases) based on
    // phaseInfo[0] and subcarrier.
    for (int ph = 0; ph < 4; ph++) {
        config.phase_sin[ph] = sinf(2.0f * M_PI * ((config.subcarrier * (ph % 4)) + config.phaseInfo[0]));
        config.phase_cos[ph] = cosf(2.0f * M_PI * ((config.subcarrier * (ph % 4)) + config.phaseInfo[0]));
        printf("phase_sin[%d]: %.6f, phase_cos[%d]: %.6f\n", ph, config.phase_sin[ph], ph, config.phase_cos[ph]);
    }

    // 4 phases
    // pixelYUV[2][4][3]
    // 2 input pixel values (0, 1)
    // 4 phases (0, 1, 2, 3)
    // 3 YUV components (Y, U, V)

    for (int ph = 0; ph < 4; ph++) {
        config.pixelYUV[0][ph][0] = 0.0f;
        config.pixelYUV[0][ph][1] = 0.0f;
        config.pixelYUV[0][ph][2] = 0.0f;

        config.pixelYUV[1][ph][0] = 1.0f;
        config.pixelYUV[1][ph][1] = config.phase_sin[ph];
        config.pixelYUV[1][ph][2] = config.phase_cos[ph];
        printf("pixelYUV[1][%d][0]: %.6f, pixelYUV[1][%d][1]: %.6f, pixelYUV[1][%d][2]: %.6f\n", ph, config.pixelYUV[1][ph][0], ph, config.pixelYUV[1][ph][1], ph, config.pixelYUV[1][ph][2]);
        printf("pixelYUV[0][%d][0]: %.6f, pixelYUV[0][%d][1]: %.6f, pixelYUV[0][%d][2]: %.6f\n", ph, config.pixelYUV[0][ph][0], ph, config.pixelYUV[0][ph][1], ph, config.pixelYUV[0][ph][2]);
    }
}

/** 
 * the Lookup table - 4 phases, 2 ^ (num_taps * 2) bits.
 */
alignas(64) RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];


// Process a single scanline of Apple II video data
RGBA_t  processAppleIIScanline_lut3(
    uint32_t inputBits,         // Input width bytes of luminance data
    int pixelposition
) 
{
    // First pass: Convert input to YIQ representation
    //float *yiq = &yiqBuffer[(pixelposition - NUM_TAPS) * 3];

    float oy = 0.0f;
    float oi = 0.0f;
    float oq = 0.0f;

    //for (int x = pixelposition - NUM_TAPS; x <= pixelposition + NUM_TAPS; x++) 
    for (int offset = - NUM_TAPS; offset <= NUM_TAPS; offset++)
    {
        int x = pixelposition + offset;
        int coeffIdx = std::abs(offset);
        bool bit = (inputBits & 1); // this might be backwards.
        float y = config.pixelYUV[bit][x % 4][0];
        float i = config.pixelYUV[bit][x % 4][1];
        float q = config.pixelYUV[bit][x % 4][2];

        oy += y * config.filterCoefficients[coeffIdx][0];  // Y
        oi += i * config.filterCoefficients[coeffIdx][1];  // I
        oq += q * config.filterCoefficients[coeffIdx][2];  // Q

        inputBits >>= 1; // this might be backwards.
    }
 
    // Convert pixel to RGB
    // Apply matrix and offset (matrix is 3x3, stored in row-major format)
    float r = config.decoderMatrix[0] * oy + config.decoderMatrix[1] * oi + config.decoderMatrix[2] * oq /* + offset[0] */;
    float g = config.decoderMatrix[3] * oy + config.decoderMatrix[4] * oi + config.decoderMatrix[5] * oq /* + offset[1] */;
    float b = config.decoderMatrix[6] * oy + config.decoderMatrix[7] * oi + config.decoderMatrix[8] * oq /* + offset[2] */;
    
    RGBA_t emit;
    // Clamp values and convert to 8-bit
    emit.r = std::clamp(static_cast<int>(r * 255), 0, 255);
    emit.g = std::clamp(static_cast<int>(g * 255), 0, 255);
    emit.b = std::clamp(static_cast<int>(b * 255), 0, 255);
    emit.a = 255;

    return emit;
}

/** 
 * extract the following into variables in the ntsc_config:
 * videoSaturation: 1.0f
 * videoHue: 0.05f
 * videoBrightness: 0.0f
 * whenever you change these values, re-call init_hgr_LUT().
 */
void init_hgr_LUT()
{
    // identity matrix?
    Matrix3x3 decoderMatrix(
        1, 0, 0,
        0, 1, 0,
        0, 0, 1);

    Matrix3x3 yiqMatrix(
        1, 0, 1.13983,
        1, -0.39465, -0.58060,
        1, 2.03206, 0
    );

    // saturation should be 0 to 1.0 
    // 0 is basically monochrome. transposing row/col here does not matter.
    //float videoSaturation = 1.0f;
    Matrix3x3 saturationMatrix(
        1, 0, 0,
        0, config.videoSaturation, 0,
        0, 0, config.videoSaturation
    );
    // if hue is 0, then hueMatrix becomes identity matrix. (transposing r
    float videoHue = 2 * (float)M_PI * config.videoHue;
    Matrix3x3 hueMatrix(
        1, 0, 0,
        0, cosf(videoHue), -sinf(videoHue),
        0, sinf(videoHue), cosf(videoHue)
    );

    decoderMatrix.multiply(saturationMatrix);
    decoderMatrix.multiply(hueMatrix);
    decoderMatrix.multiply(yiqMatrix);
    decoderMatrix.print();
    config.decoderMatrix = decoderMatrix;

    for (int phaseCount = 0; phaseCount < 4; phaseCount++)
    {
        for (uint32_t bitCount = 0; bitCount < (1 << ((NUM_TAPS * 2) + 1)); bitCount++)
        {
            //  set the bit pattern on either size of the center (16 + phaseCount)

            int center = (16 + phaseCount);

            RGBA_t rval = processAppleIIScanline_lut3(bitCount,/*  outputImage, */ center /* , width, yiqBuffer, filteredYiq */);
            g_hgr_LUT[phaseCount][bitCount] = rval;  //  Pull out the center pixel and save it.
        }
    }
}

/** Generate a 'frame' (i.e., a group of 8 scanlines) of video output data using the lookup table.  */
void processAppleIIFrame_LUT (
    uint8_t* frameData,         // 560x192 bytes - gray bitstream data
    RGBA_t * outputImage,          // Will be filled with 560x192 RGBA pixels
    int y_start,
    int y_end
)
{
    int mask = ((1 << ((NUM_TAPS * 2) + 1)) - 1);

    // Process each scanline
    for (int y = y_start; y < y_end; y++)
    {
        uint32_t bits = 0;

        // for x = 0, we need bits preloaded with the first NUM_TAPS+1 bits in 
        // 16-8, and 0's in 0-7.
        // if num_taps = 6, 
        // 11111 1X000000
        for (int i = 0; i < NUM_TAPS; i++)
        {
            bits = bits >> 1;
            if (frameData[i] != 0)
                bits = bits | (1 << ((NUM_TAPS*2)));
        }
        
        // Process the scanline
        for (int x = 0; x < config.width; x++)
        {
            bits = bits >> 1;
            if ((x < config.width-NUM_TAPS) && (frameData[NUM_TAPS] != 0)) // lookahead..
                bits = bits | (1 << ((NUM_TAPS*2)));

            int phase = x % 4;

            //  Use the phase and the bits as the index
            outputImage[0] = g_hgr_LUT[phase][bits];
            outputImage++;
            frameData++;
        }
    }
}

/**
 * Mono mode - just convert the bitstream to RGBA white (for now, I will grab the mono color later)
 */
void processAppleIIFrame_Mono (
    uint8_t *frameData,         // 560x192 bytes - gray bitstream data
    RGBA_t *outputImage,          // Will be filled with 560x192 RGBA pixels
    int y_start,
    int y_end,
    RGBA_t color_value
)
{
    static RGBA_t p_black = RGBA_t::make(0x00, 0x00, 0x00, 0xFF);

    // Process each scanline
    for (int y = y_start; y < y_end; y++)
    {
        for (int x = 0; x < config.width; x++) {
            if (frameData[0]) {
                outputImage[0] = color_value;
            } else {
                outputImage[0] = p_black;
            }
            outputImage++;
            frameData++;
        }
    }
}
