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
#include <chrono>
#include <stdio.h>
#include "Matrix3x3.hpp"
#include "display/types.hpp"
#include "display/ntsc.hpp"

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

// Apply FIR filter to YIQ components - completely revised implementation
inline void applyFilter(//const std::vector<std::vector<float>>& coefficients, 
                const std::vector<float>& yiqBuffer,
                int x, int width,
                float* output) {
    // Initialize output to zero
    output[0] = output[1] = output[2] = 0.0f;
    
    // Apply center tap (c0)
    const float* center = &yiqBuffer[x * 3];
    output[0] += center[0] * config.filterCoefficients[0][0];  // Y
    output[1] += center[1] * config.filterCoefficients[0][1];  // I
    output[2] += center[2] * config.filterCoefficients[0][2];  // Q
    
    // Apply symmetric taps (c1 through c8)
    for (int i = 1; i <= NUM_TAPS; i++) {
        // Safely access left and right neighbors with bounds checking
        int leftIdx = x - i;
        int rightIdx = x + i;
        
        // Apply left neighbor if in bounds
        if (leftIdx >= 0) {
            const float* left = &yiqBuffer[leftIdx * 3];
            output[0] += left[0] * config.filterCoefficients[i][0];  // Y
            output[1] += left[1] * config.filterCoefficients[i][1];  // I
            output[2] += left[2] * config.filterCoefficients[i][2];  // Q
        }
        
        // Apply right neighbor if in bounds
        if (rightIdx < width) {
            const float* right = &yiqBuffer[rightIdx * 3];
            output[0] += right[0] * config.filterCoefficients[i][0];  // Y
            output[1] += right[1] * config.filterCoefficients[i][1];  // I
            output[2] += right[2] * config.filterCoefficients[i][2];  // Q
        }
    }
}

// TODO: see if this is correct and is the source of the transposed row/col wise matrices issue.
// it's a different implementation than above. 
// Function to convert YIQ to RGB using a decoding matrix
inline void yiqToRgb(const float yiq[3], /* const Matrix3x3& matrix,  */ /* const float offset[3], */ RGBA &rgba) {
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

// Function to display YIQ buffer values
void displayYIQBuffer(const std::vector<float>& yiqBuffer, int width, int startX, int count) {
    printf("YIQ Buffer Values (x=%d to %d):\n", startX, startX + count - 1);
    printf("   X   |     Y     |     I     |     Q     \n");
    printf("-------+-----------+-----------+-----------\n");
    
    for (int i = 0; i < count; i++) {
        int x = startX + i;
        if (x >= width) break;
        
        printf(" %5d | %9.6f | %9.6f | %9.6f\n", 
               x, 
               yiqBuffer[x * 3],     // Y
               yiqBuffer[x * 3 + 1], // I
               yiqBuffer[x * 3 + 2]  // Q
        );
    }
    printf("\n");
}

// Apply FIR filter to YIQ components - completely revised implementation
inline void applyFilter_mod(//const std::vector<std::vector<float>>& coefficients, 
    const std::vector<float>& yiqBuffer,
    int x, int width,
    float* output) {
    // Initialize output to zero
    output[0] = output[1] = output[2] = 0.0f;

    // Apply center tap (c0)
    const float* center = &yiqBuffer[x * 3];
    output[0] += center[0] * config.filterCoefficients[0][0];  // Y
    output[1] += center[1] * config.filterCoefficients[0][1];  // I
    output[2] += center[2] * config.filterCoefficients[0][2];  // Q

    // Apply symmetric taps (c1 through c8)
    for (int i = 1; i <= NUM_TAPS; i++) {     //  NUM_TAPS
        // Safely access left and right neighbors with bounds checking
        int leftIdx = x - i;
        int rightIdx = x + i;

        // Apply left neighbor if in bounds
        if (leftIdx >= 0) {
            const float* left = &yiqBuffer[leftIdx * 3];
            output[0] += left[0] * config.filterCoefficients[i][0];  // Y
            output[1] += left[1] * config.filterCoefficients[i][1];  // I
            output[2] += left[2] * config.filterCoefficients[i][2];  // Q
        }

        // Apply right neighbor if in bounds
        if (rightIdx < width) {
            const float* right = &yiqBuffer[rightIdx * 3];
            output[0] += right[0] * config.filterCoefficients[i][0];  // Y
            output[1] += right[1] * config.filterCoefficients[i][1];  // I
            output[2] += right[2] * config.filterCoefficients[i][2];  // Q
        }
    }
}

// Process a single scanline of Apple II video data
void processAppleIIScanline_mod(
    uint8_t* inputScanline,  // Input 560 bytes of luminance data
    RGBA* outputScanline,         // Output 560 RGBA pixels
    int scanlineY                             // Current scanline index
) {

    // Generate phase information for this scanline - should tweak for every other scanline
    //std::vector<float> phaseInfo = generatePhaseInfo(scanlineY, config.colorBurst);

    // Create temporary buffer for YIQ components
    std::vector<float> yiqBuffer(config.width * 3);  // 3 components per pixel (Y, I, Q)

    // First pass: Convert input to YIQ representation
    for (int x = 0; x < config.width; x++) {
        // Calculate phase for this pixel. Should use addition instead of all these multiplies.
        float phase = 2.0f * M_PI * ((config.subcarrier * (x % 4)) + config.phaseInfo[0]);
        //printf("scanlineY: %d, x: %d, phase: %.6f\n", scanlineY, x, phase);
        // Process pixel to YIQ
        float* yiq = &yiqBuffer[x * 3];
        processPixel(inputScanline[x], phase, config.phaseInfo[1], yiq);
    }

    // Display YIQ buffer values for the first few pixels if this is the first scanline
#if 0
    if (scanlineY == 0) {
        displayYIQBuffer(yiqBuffer, config.width, 0, 10);  // Show first 10 pixels
    }
#endif

    // Second pass: Apply FIR filter to each pixel
    std::vector<float> filteredYiq(3);  // Temporary storage for filtered YIQ

    for (int x = 0; x < config.width; x++) {
        // Apply filter using the whole buffer
        applyFilter_mod(/* config.filterCoefficients, */ yiqBuffer, x, config.width, filteredYiq.data());

        // Convert to RGB
        yiqToRgb(filteredYiq.data(), /* config.decoderMatrix, */ /* config.decoderOffset, */ outputScanline[x]);
    }
}

// Process a single scanline of Apple II video data
void processAppleIIScanline(
    uint8_t *inputScanline,  // Input 560 bytes of luminance data
    RGBA *outputScanline,         // Output 560 RGBA pixels
    int scanlineY                             // Current scanline index
) {

    // Generate phase information for this scanline - should tweak for every other scanline
    //std::vector<float> phaseInfo = generatePhaseInfo(scanlineY, config.colorBurst);
    
    // Create temporary buffer for YIQ components
    std::vector<float> yiqBuffer(config.width * 3);  // 3 components per pixel (Y, I, Q)
    
    // First pass: Convert input to YIQ representation
    for (int x = 0; x < config.width; x++) {
        // Calculate phase for this pixel. Should use addition instead of all these multiplies.
        float phase = 2.0f * M_PI * (config.subcarrier * x + config.phaseInfo[0]);
        //printf("scanlineY: %d, x: %d, phase: %.6f\n", scanlineY, x, phase);
        // Process pixel to YIQ
        float* yiq = &yiqBuffer[x * 3];
        processPixel(inputScanline[x], phase, config.phaseInfo[1], yiq);
    }
    
    // Display YIQ buffer values for the first few pixels if this is the first scanline
#if 0
    if (scanlineY == 0) {
        displayYIQBuffer(yiqBuffer, config.width, 0, 10);  // Show first 10 pixels
    }
#endif
    
    // Second pass: Apply FIR filter to each pixel
    std::vector<float> filteredYiq(3);  // Temporary storage for filtered YIQ
    
    for (int x = 0; x < config.width; x++) {
        // Apply filter using the whole buffer
        applyFilter(/* config.filterCoefficients, */ yiqBuffer, x, config.width, filteredYiq.data());
        
        // Convert to RGB
        yiqToRgb(filteredYiq.data(), /* config.decoderMatrix, */ /* config.decoderOffset, */ outputScanline[x]);
    }
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

   config = {
        .width = FRAME_WIDTH,
        .height = FRAME_HEIGHT,
        .colorBurst = colorBurst,
        .subcarrier = subcarrier,
            // Set up filter coefficients based on measured YIQ filter response
        .filterCoefficients = {
            {0.279408f, 0.374189f, 0.260298f},  // c0
            {0.235926f, 0.341248f, 0.247879f},  // c1
            {0.134616f, 0.256477f, 0.213725f},  // c2
            {0.036655f, 0.153548f, 0.166016f},  // c3
            {-0.015383f, 0.066575f, 0.115090f}, // c4
            {-0.022104f, 0.013702f, 0.070082f}, // c5
            {-0.009993f, -0.006718f, 0.036478f},// c6
            {-0.000718f, -0.007999f, 0.015433f},// c7
            {0.001297f, -0.003928f, 0.005148f}  // c8
        },
        .decoderOffset = {0.0f, 0.0f, 0.0f} // TODO: first value should brightness. -1 to +1.
    };
    generatePhaseInfo(/* scanlineY */ 0, colorBurst);
}


RGBA g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

// Process a single scanline of Apple II video data
// used to help initialize the LUT
void processAppleIIScanline_init(
    uint8_t* inputScanline,         // Input width bytes of luminance data
    RGBA* outputScanline,           // Output width RGBA pixels
    int width                            
    ) 
{
    // Generate phase information for this scanline - should tweak for every other scanline
    //std::vector<float> phaseInfo = generatePhaseInfo(scanlineY, config.colorBurst);

    // Create temporary buffer for YIQ components
    std::vector<float> yiqBuffer(width * 3);  // 3 components per pixel (Y, I, Q)

    // First pass: Convert input to YIQ representation
    for (int x = 0; x < width; x++) 
    {
        // Calculate phase for this pixel. Should use addition instead of all these multiplies.
        float phase = 2.0f * M_PI * ((config.subcarrier * (x % 4)) + config.phaseInfo[0]);
        //printf("scanlineY: %d, x: %d, phase: %.6f\n", scanlineY, x, phase);
        // Process pixel to YIQ
        float* yiq = &yiqBuffer[x * 3];
        processPixel(inputScanline[x], phase, config.phaseInfo[1], yiq);
    }

    // Second pass: Apply FIR filter to each pixel
    std::vector<float> filteredYiq(3);  // Temporary storage for filtered YIQ

    for (int x = 0; x < width; x++) 
    {
        // Apply filter using the whole buffer
        applyFilter_mod(/* config.filterCoefficients, */ yiqBuffer, x, width, filteredYiq.data());

        // Convert to RGB
        yiqToRgb(filteredYiq.data(), /* config.decoderMatrix, */ /* config.decoderOffset, */ outputScanline[x]);
    }
}

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
    float videoSaturation = 1.0f;
    Matrix3x3 saturationMatrix(
        1, 0, 0,
        0, videoSaturation, 0,
        0, 0, videoSaturation
    );
    // if hue is 0, then hueMatrix becomes identity matrix. (transposing r
    float videoHue = 2 * (float)M_PI * 0.05f;
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

    int width = 48;     //   config.width
    uint8_t* frameData = new uint8_t[width];       //  This leaks
    RGBA* outputImage = new RGBA[width];       //  This leaks

    for (int phaseCount = 0; phaseCount < 4; phaseCount++)
    {
        for (uint32_t bitCount = 0; bitCount < (1 << ((NUM_TAPS * 2) + 1)); bitCount++)
        {
            //  clear the data
            for (int i = 0; i < width; i++)
                frameData[i] = 0xff;

            //  set the bit pattern on either size of the center (16 + phaseCount)
            uint32_t bits = bitCount;
            int center = (16 + phaseCount);
            int lefIndex = (center - NUM_TAPS);
            int rightIndex = (center + NUM_TAPS);

            for (int i = lefIndex; i <= rightIndex; i++)
            {
                //  Add the bits to the buffer, LSB on the left
                if ((bits & 0x01) != 0)
                    frameData[i] = 0xFF;
                else
                    frameData[i] = 0x00;

                bits = bits >> 1;
            }

            processAppleIIScanline_init(frameData, outputImage, width);

            g_hgr_LUT[phaseCount][bitCount] = outputImage[center];  //  Pull out the center pixel and save it.

            // printf("LUT: %d %d : %d %d %d %d %d %d\n", phaseCount, bitCount, g_hgr_LUT[phaseCount][bitCount].r, g_hgr_LUT[phaseCount][bitCount].g, g_hgr_LUT[phaseCount][bitCount].b, outputImage[center].r, outputImage[center].g, outputImage[center].b);
        }
    }
}

void processAppleIIFrame_LUT(
    uint8_t* frameData,         // 560x192 bytes - gray bitstream data
    RGBA* outputImage,          // Will be filled with 560x192 RGBA pixels
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


