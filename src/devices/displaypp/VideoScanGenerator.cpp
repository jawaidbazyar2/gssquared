
#include "Device_ID.hpp"

#include "computer.hpp"
#include "frame/Frames.hpp"
#include "VideoScannerII.hpp"
#include "VideoScanGenerator.hpp"

VideoScanGenerator::VideoScanGenerator()
{
    build_hires40Font(true);
}

void VideoScanGenerator::build_hires40Font(bool delayEnabled) 
{
    for (int i = 0; i < 2 * CHAR_NUM; i++)
    {
        uint8_t value = (i & 0x7f) << 1 | (i >> 8);
        bool delay = delayEnabled && (i & 0x80);
        
        for (int x = 0; x < CHAR_WIDTH; x++)
        {
            bool bit = (value >> ((x + 2 - delay) >> 1)) & 0x1;
            
            hires40Font[i * CHAR_WIDTH + x] = bit ? 1 : 0;
        }
    }
}

void VideoScanGenerator::generate_frame(FrameScan560 *frame_scan, Frame560 *frame_byte)
{
    for (uint16_t vcount = 0; vcount < 192; ++vcount)
    {
        frame_scan->set_line(vcount);
        frame_byte->set_line(vcount);
        uint8_t lastByte = 0x00; // for hires

        for (int hcount = 0; hcount < 40; hcount++) {

            Scan_t scan = frame_scan->pull();

            switch (scan.mode) {
                case VM_LORES: {
                        if (hcount == 0) {
                            for (int i = 0; i < 7; i++) {
                                frame_byte->push(0);
                            }
                            frame_byte->set_color_mode(vcount, COLORBURST_ON);
                        }
                        uint8_t tchar = scan.mainbyte;
                        
                        if (vcount & 4) { // if we're in the second half of the scanline, shift the byte right 4 bits to get the other nibble
                            tchar = tchar >> 4;
                        }
                        uint16_t pixeloff = (hcount * 14) % 4;
        
                        for (int bits = 0; bits < CELL_WIDTH; bits++) {
                            uint8_t bit = ((tchar >> pixeloff) & 0x01);
                            frame_byte->push(bit);
                            pixeloff = (pixeloff + 1) % 4;
                        }
                    }
                    break;
                case VM_HIRES: {
                        if (hcount == 0) {
                            for (int i = 0; i < 7; i++) {
                                frame_byte->push(0);
                            }
                            frame_byte->set_color_mode(vcount, COLORBURST_ON);
                        }
                        uint8_t byte = scan.mainbyte;
                        size_t fontIndex = (byte | ((lastByte & 0x40) << 2)) * CHAR_WIDTH; // bit 6 from last byte selects 2nd half of font
                
                        for (int i = 0; i < 14; i++) {
                            frame_byte->push(hires40Font[fontIndex + i]);
                        }
                        lastByte = byte;
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

