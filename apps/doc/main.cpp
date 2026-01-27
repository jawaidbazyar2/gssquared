#include <algorithm>
#include <cstring>
#include <cassert>

#include "devices/es5503/ensoniq.hpp"
#include "computer.hpp"
#include "devices/es5503/soundglu.hpp"

DebugFormatter * debug_ensoniq2(ensoniq_state_t *st) {
    DebugFormatter *df = new DebugFormatter();
    df->addLine("Control: %02X Address: %04X", st->soundctl, (st->soundadrh << 8) | st->soundadrl);
    //df->addLine("  Sound Data: %02X", st->sounddata);
    ES5503 *chip = st->chip;
    df->addLine("E0: %02X   E1: %02X   E2: %02X", chip->get_rege0(), chip->get_rege1(), 0  /* , chip->get_adc_callback() */ );

    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    df->addLine("OutRate: %u Hz  OSCs: %d", es5503_output_rate, chip->get_oscsenabled());

    df->addLine("Osc Freq WtSize Ctrl Vol Data WtPtr WtSize Res Acc       Irq");
    for (int o = 0; o < 2; o++) {
        Oscillator *osc = chip->get_oscillator(o);
        df->addLine(" %2d %04X   %04X  %02X  %02X  %02X   %04X  %02X    %02X  %08X %02X", 
            o, osc->freq, osc->wtsize, osc->control, osc->vol, 
            osc->data, osc->wavetblpointer, osc->wavetblsize, osc->resolution, 
            osc->accumulator, osc->irqpend);
    }
    return df;
}

int main(int argc, char **argv) {
    ensoniq_state_t *st = new ensoniq_state_t();
    
	SDL_Init(SDL_INIT_AUDIO);
    
    // for info purposes, print out the available devices and their formats.
    int num_devices = 0;
    SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&num_devices);
    for (int i = 0; i < num_devices; i++) {
        SDL_AudioSpec spec;
        SDL_GetAudioDeviceFormat(devices[i], &spec, NULL);
        printf("AudioDevice %d: %s %d\n", i, SDL_GetAudioDeviceName(devices[i]), spec.freq);

    }
    SDL_free(devices);

    // Allocate 64KB DOC RAM
    st->doc_ram = new uint8_t[0x10000];
    std::memset(st->doc_ram, 0x80, 0x10000); // 0x80 is silence

    // Allocate buffer large enough for maximum samples per frame
    // Max rate ~298kHz at 59.92 fps = ~4972 samples, add some headroom
    st->audio_buffer = new int16_t[16384];
    
    // Create and initialize ES5503 chip
    ES5503 *chip = new ES5503();
    st->chip = chip;
    st->chip->init(7159090, 48000, 1);  // Apple IIgs clock rate, 48kHz stereo
    st->chip->set_wave_memory(st->doc_ram);
    
    // TODO: Set up IRQ callback
    // st->chip->set_irq_callback([](bool state) { /* handle IRQ */ });

    int dev_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (dev_id == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return(0);
    }

    // Calculate frame rate
    //st->frame_rate = (double)computer->clock->c14M_per_second / (double)computer->clock->c14M_per_frame;
    
    // Calculate ES5503 output rate and set up SDL stream
    uint32_t es5503_output_rate = st->chip->calculate_output_rate();
    
    // Calculate initial samples per frame
    st->samples_per_frame = (float)es5503_output_rate / st->frame_rate;
    st->samples_accumulated = 0.0f;
    
    SDL_AudioSpec spec;
    spec.freq = es5503_output_rate;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;

    printf("Creating audio stream with freq: %d samps/frame: %f\n", spec.freq, st->samples_per_frame);

    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, NULL);
    if (!stream) {
        printf("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(dev_id, stream)) {  /* once bound, it'll start playing when there is data available! */
        printf("Failed to bind stream to device: %s", SDL_GetError());
    }
    st->stream = stream;
    
    // Set the stream pointer in the chip so it can update the rate when oscillators change
    st->chip->set_sdl_stream(stream);

    for (int i = 0; i < 256; i++) {
        uint8_t ssamp = 128.0f + (127.0f * sin((float)i/(float)(128/M_PI)));
        st->doc_ram[i] = ssamp;
    }

    // set num oscs to 31
    chip->write(0xE1, 0x1F<<1);
    // set freq to 3E8
    chip->write(0x00, 0xE8);
    chip->write(0x20, 0x03);
    // set wtsize = 0100
    // set ctrl = 0
    chip->write(0xA0, 0x00);
    chip->write(0xC0, 0x00);
    // set vol = 0x78 (120)
    chip->write(0x40, 0x78);
    // set WtPtr = 0
    // set WtSize = 0 (whu?)

    DebugFormatter *df = debug_ensoniq2(st);
    for (const std::string& line : df->getLines()) {
        printf("%s\n", line.c_str());
    }
    delete df;
    
    while (1) {
        st->chip->generate_samples(st->audio_buffer, st->samples_per_frame);
        SDL_PutAudioStreamData(st->stream, st->audio_buffer, st->samples_per_frame * sizeof(int16_t));
        for (int i = 0; i < st->samples_per_frame; i++) {
            printf("%04X ", (uint16_t)st->audio_buffer[i]);
        }
        printf("\n");
        //printf("queued: %d\n", SDL_GetAudioStreamAvailable(st->stream));
        SDL_Delay(14);
    }
}