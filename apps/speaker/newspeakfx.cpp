#include <cstdint>
#include <cstdio>
#include <iostream>

#include "clock.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "event_poll.hpp"
#include "SpeakerFX.hpp"

uint64_t debug_level;
bool write_output = false;

FILE *wav_file = NULL;

#if 0
FILE *create_wav_file(const char *filename) {
    FILE *wav_file = fopen(filename, "wb");
    if (!wav_file) {
        std::cerr << "Error: Could not open wav file\n";
        return NULL;
    }

// write a WAV header:
    // RIFF header
    fwrite("RIFF", 1, 4, wav_file);
    
    // File size (to be filled later)
    uint32_t file_size = 0;
    fwrite(&file_size, 4, 1, wav_file);
    
    // WAVE header
    fwrite("WAVE", 1, 4, wav_file);
    
    // Format chunk
    fwrite("fmt ", 1, 4, wav_file);
    
    // Format chunk size (16 bytes)
    uint32_t fmt_chunk_size = 16;
    fwrite(&fmt_chunk_size, 4, 1, wav_file);
    
    // Audio format (1 = PCM)
    uint16_t audio_format = 1;
    fwrite(&audio_format, 2, 1, wav_file);
    
    // Number of channels (1 = mono)
    uint16_t num_channels = 1;
    fwrite(&num_channels, 2, 1, wav_file);
    
    // Sample rate (44100Hz)
    uint32_t sample_rate = SAMPLE_RATE;
    fwrite(&sample_rate, 4, 1, wav_file);
    
    // Byte rate = SampleRate * NumChannels * BitsPerSample/8
    uint32_t byte_rate = sample_rate * num_channels * 16/8;
    fwrite(&byte_rate, 4, 1, wav_file);
    
    // Block align = NumChannels * BitsPerSample/8
    uint16_t block_align = num_channels * 16/8;
    fwrite(&block_align, 2, 1, wav_file);
    
    // Bits per sample
    uint16_t bits_per_sample = 16;
    fwrite(&bits_per_sample, 2, 1, wav_file);
    
    // Data chunk header
    fwrite("data", 1, 4, wav_file);
    
    // Data chunk size (to be filled later)
    uint32_t data_size = 0;
    fwrite(&data_size, 4, 1, wav_file);

    return wav_file;
}

void finalize_wav_file(FILE *wav_file) {
    long file_size = ftell(wav_file);
    uint32_t riff_size = file_size - 8; // Exclude "RIFF" and its size field
    uint32_t data_size = file_size - 44; // Exclude header size (44 bytes)

    // Seek to RIFF size field and update
    fseek(wav_file, 4, SEEK_SET);
    fwrite(&riff_size, 4, 1, wav_file);

    // Seek to data size field and update
    fseek(wav_file, 40, SEEK_SET);
    fwrite(&data_size, 4, 1, wav_file);

    // Close the file
    fclose(wav_file);
}
#endif

void event_poll_local(cpu_state *cpu) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch (event.type) {

        }
    }
}


void usage(const char *exe) {
    std::cerr << "Usage: " << exe << " [-w output_wav_file] [-f synth_frequency] [-i input_rate] [-o output_rate] [recording_file]\n";
    exit(1);
}

int main(int argc, char **argv) {
    debug_level = DEBUG_SPEAKER;

    /*
        -d : debug mode
        -i : specify input rate (input timing rate in Hz)
        -o : specify SDL3 output rate (modern sample audio rate in Hz)
        -w filename : write output to WAV file
        filename : pre-recorded event file to playback.
    */
    bool debug_mode = false;
    bool log_results = false;
    int input_rate = 1020484;
    int output_rate = 44100;
    uint32_t synth_frequency = 1000;
    const char *wav_filename = nullptr;
    const char *playback_file = nullptr;
    int opt;
    while ((opt = getopt(argc, argv, "dlf:i:o:w:")) != -1) {
        switch (opt) {
            case 'd':
                debug_mode = true;
                break;
            case 'i':
                input_rate = atoi(optarg);
                break;
            case 'o':
                output_rate = atoi(optarg);
                break;
            case 'f':
                synth_frequency = atoi(optarg);
                break;
            case 'w':
                wav_filename = optarg;
                break;
            case 'l':
                log_results = true;
                break;
        }
    }
    
    // Get the optional playback filename from remaining arguments
    if (optind < argc) {
        playback_file = argv[optind];
    }
    
    int recording_file_index = 1;


    cpu_state *cpu = new cpu_state(PROCESSOR_65C02);
    
    SpeakerFX *speaker = new SpeakerFX(input_rate, output_rate, 2'000'000, 1024);
    
    select_system_clock(CLOCK_SET_US);

    MMU_II *mmu = new MMU_II(256, 48*1024, nullptr);
    cpu->set_mmu(mmu);

    clock_mode_info_t *clock = &system_clock_mode_info[CLOCK_1_024MHZ];

    /* init_mb_speaker_local(speaker_state, clock); */
    printf("clock mode         : %llu\n", clock->hz_rate);
    printf("c14M_per_second    : %llu\n", clock->c14M_per_second);
    printf("c14M_per_frame     : %llu\n", clock->c14M_per_frame);
    printf("c_14M_per_cpu_cycle: %llu\n", clock->c_14M_per_cpu_cycle);
    printf("extra_per_scanline : %llu\n", clock->extra_per_scanline);
    printf("cycles_per_scanline: %llu\n", clock->cycles_per_scanline);
    printf("us_per_frame_even  : %llu\n", clock->us_per_frame_even);
    printf("us_per_frame_odd   : %llu\n", clock->us_per_frame_odd);
    printf("eff_cpu_rate       : %llu\n", clock->eff_cpu_rate);

    if (playback_file) {
        if (!speaker->load_events(playback_file)) {
            printf("Error: Could not load events\n");        
            return 1;
        }
    } else {
        printf("Synthesizing %dHz events for 10 seconds\n", synth_frequency);
        speaker->synthesize_events(synth_frequency, 10);
    }

    speaker->print();

    speaker->start();

    // skip to the first event
    cpu->cycles = speaker->get_first_event();
    speaker->fast_forward(cpu->cycles);

    uint32_t samples_per_frame = speaker->samples_per_frame;

    uint64_t cycles_per_frame = (speaker->cycles_per_sample * samples_per_frame) >> FRACTION_BITS; // only used to estimate the number of frames.

    /* if (write_output) {
        wav_file = create_wav_file("test.wav");
    } */
    uint64_t cycle_window_last = 0;
    uint64_t num_frames = ((speaker->get_last_event() - speaker->get_first_event()) / cycles_per_frame);

    printf("first_event: %llu last_event: %llu\n", speaker->get_first_event(), speaker->get_last_event());
    printf("num_frames: %llu\n", num_frames);

    uint64_t frame_timer;
    uint64_t total_time = 0;
    uint64_t lowest_processing_time = 9999999999999999, highest_processing_time = 0;

    for (int i = 0; i < num_frames; i++) {
        event_poll_local(cpu);

        frame_timer = SDL_GetTicksNS();
        
        uint64_t samps = speaker->generate_and_queue(samples_per_frame);
        /* if (write_output) {
            fwrite(speaker_state->working_buffer, sizeof(int16_t), samps, wav_file);
        } */
        cycle_window_last = cpu->cycles;
        cpu->cycles += cycles_per_frame;
               
        uint64_t processing_time = SDL_GetTicksNS() - frame_timer;
        total_time += processing_time;
        if (processing_time < lowest_processing_time) {
            lowest_processing_time = processing_time;
        }
        if (processing_time > highest_processing_time) {
            highest_processing_time = processing_time;
        }

        printf("frame %d\r", i);
        fflush(stdout);

        //SDL_Delay(12);
        while (SDL_GetTicksNS() < frame_timer + 16667000) {
            // busy wait to the next frame.
        }

    }
    printf("\n");
    printf("total_time: %llu \n", total_time );
    printf("lowest_processing_time: %llu highest_processing_time: %llu\nAverage per frame: %llu\n", lowest_processing_time, highest_processing_time, total_time / num_frames);
    int queued = 0;
    if (!write_output) {
        while ((queued = speaker->get_queued_samples()) > 0) {
            //printf("queued: %d\n", queued);
            event_poll_local(cpu);
            printf("queued: %d\r", queued);
            fflush(stdout);    
        }
    }
    /* if (write_output) {
        finalize_wav_file(wav_file);
    } */

    if (log_results) {
        FILE *ff = fopen("results.txt", "a+");
        fprintf(ff, "%s,%d,%d,%llu,%llu,%llu\n", playback_file, input_rate, output_rate, lowest_processing_time, total_time / num_frames, highest_processing_time);
        fclose(ff);
    }

    return 0;
}