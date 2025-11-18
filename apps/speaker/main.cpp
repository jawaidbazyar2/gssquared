#include <cstdint>
#include <cstdio>
#include <iostream>

#include "cpu.hpp"
#include "debug.hpp"
#include "devices/speaker/speaker.hpp"
#include "event_poll.hpp"

#define SPEAKER_EVENT_LOG_SIZE 16384
//uint64_t speaker_event_log[SPEAKER_EVENT_LOG_SIZE];

uint64_t debug_level;
bool write_output = false;

FILE *wav_file = NULL;

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

void event_poll_local(cpu_state *cpu) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch (event.type) {

        }
    }
}

void *get_module_state(cpu_state *cpu, module_id_t module_id) {
    return cpu->module_store[module_id];
}

void set_module_state(cpu_state *cpu, module_id_t module_id, void *state) {
    cpu->module_store[module_id] = state;
}

/* void register_C0xx_memory_read_handler(unsigned short address, unsigned char (*read_handler)(cpu_state*, unsigned short)) {
}

void register_C0xx_memory_write_handler(uint16_t address, memory_write_handler handler) {
} */

void usage(const char *exe) {
    std::cerr << "Usage: " << exe << " [-w] <recording file>\n";
    exit(1);
}

int main(int argc, char **argv) {
    debug_level = DEBUG_SPEAKER;

    computer_t *computer = new computer_t();
    cpu_state *cpu = new cpu_state();
    computer->cpu = cpu;

    MMU_II *mmu = new MMU_II(256, 48*1024, nullptr);
    cpu->set_mmu(mmu);

    // Parse command line arguments
    if (argc < 2) {
        usage(argv[0]);
    }

    int recording_file_index = 1;
    if (strcmp(argv[1], "-w") == 0) {
        write_output = true;
        recording_file_index = 2;
        if (argc < 3) {
            usage(argv[0]);
        }
    }

    // load 'recording' file into the log
    FILE *recording = fopen(argv[recording_file_index], "r");
    if (!recording) {
        std::cerr << "Error: Could not open recording file\n";
        return 1;
    }


    init_mb_speaker(computer, SLOT_NONE);
    // load events into the event buffer that is allocated by the speaker module.
    speaker_state_t *speaker_state = (speaker_state_t *)get_module_state(cpu, MODULE_SPEAKER);
    uint64_t event;

    // skip any long silence, start playback / reconstruction at first event.
    uint64_t first_event = 0;
    uint64_t last_event = 0;

    while (fscanf(recording, "%llu", &event) != EOF) {
        speaker_state->event_buffer.add_event(event);
        if (first_event == 0) {
            first_event = event;
        }
        last_event = event;
    }
    fclose(recording);

    speaker_start(cpu);

    cpu->cycles = first_event;

    if (write_output) {
        wav_file = create_wav_file("test.wav");
    }
    uint64_t cycle_window_last = 0;
    uint64_t num_frames = ((last_event - first_event) / 17000);

    for (int i = 0; i < num_frames; i++) {
        event_poll_local(cpu);
        
        uint64_t samps = audio_generate_frame(computer, cpu, cycle_window_last, cpu->cycles);
        if (write_output) {
            fwrite(speaker_state->working_buffer, sizeof(int16_t), samps, wav_file);
        }
        cycle_window_last = cpu->cycles;
        cpu->cycles += 17000;
        
        //SDL_Delay(17);
    }
    int queued = 0;
    if (!write_output) {
        while ((queued = SDL_GetAudioStreamAvailable(speaker_state->stream)) > 0) {
            //printf("queued: %d\n", queued);
            event_poll_local(cpu);
        }
    }
    if (write_output) {
        finalize_wav_file(wav_file);
    }

    return 0;
}