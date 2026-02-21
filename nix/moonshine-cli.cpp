#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "moonshine-c-api.h"

namespace fs = std::filesystem;

struct WavData {
    std::vector<float> samples;
    int32_t sample_rate = 0;
};

static WavData load_wav(const std::string &path) {
    WavData result;

    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        fprintf(stderr, "Error: failed to open WAV file: %s\n", path.c_str());
        std::exit(1);
    }

    // Read RIFF header
    char riff_header[4];
    if (std::fread(riff_header, 1, 4, file) != 4 ||
        std::strncmp(riff_header, "RIFF", 4) != 0) {
        std::fclose(file);
        fprintf(stderr, "Error: not a RIFF file: %s\n", path.c_str());
        std::exit(1);
    }

    // Skip chunk size and check WAVE
    std::fseek(file, 4, SEEK_CUR);
    char wave_header[4];
    if (std::fread(wave_header, 1, 4, file) != 4 ||
        std::strncmp(wave_header, "WAVE", 4) != 0) {
        std::fclose(file);
        fprintf(stderr, "Error: not a WAVE file: %s\n", path.c_str());
        std::exit(1);
    }

    // Find "fmt " chunk
    char chunk_id[4];
    uint32_t chunk_size = 0;
    bool found_fmt = false;
    while (std::fread(chunk_id, 1, 4, file) == 4) {
        if (std::fread(&chunk_size, 4, 1, file) != 1) break;
        if (std::strncmp(chunk_id, "fmt ", 4) == 0) {
            found_fmt = true;
            break;
        }
        std::fseek(file, chunk_size, SEEK_CUR);
    }
    if (!found_fmt) {
        std::fclose(file);
        fprintf(stderr, "Error: no fmt chunk in: %s\n", path.c_str());
        std::exit(1);
    }

    // Read fmt chunk
    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0, byte_rate = 0;
    uint16_t block_align = 0;
    if (chunk_size < 16) {
        std::fclose(file);
        fprintf(stderr, "Error: fmt chunk too small in: %s\n", path.c_str());
        std::exit(1);
    }
    std::fread(&audio_format, sizeof(uint16_t), 1, file);
    std::fread(&num_channels, sizeof(uint16_t), 1, file);
    std::fread(&sample_rate, sizeof(uint32_t), 1, file);
    std::fread(&byte_rate, sizeof(uint32_t), 1, file);
    std::fread(&block_align, sizeof(uint16_t), 1, file);
    std::fread(&bits_per_sample, sizeof(uint16_t), 1, file);
    if (chunk_size > 16) std::fseek(file, chunk_size - 16, SEEK_CUR);

    // Suppress unused variable warnings
    (void)num_channels;
    (void)byte_rate;
    (void)block_align;

    if (audio_format != 1 || bits_per_sample != 16) {
        std::fclose(file);
        fprintf(stderr, "Error: only 16-bit PCM WAV files are supported\n");
        std::exit(1);
    }

    // Find "data" chunk
    bool found_data = false;
    while (std::fread(chunk_id, 1, 4, file) == 4) {
        if (std::fread(&chunk_size, 4, 1, file) != 1) break;
        if (std::strncmp(chunk_id, "data", 4) == 0) {
            found_data = true;
            break;
        }
        std::fseek(file, chunk_size, SEEK_CUR);
    }
    if (!found_data) {
        std::fclose(file);
        fprintf(stderr, "Error: no data chunk in: %s\n", path.c_str());
        std::exit(1);
    }

    // Read PCM data
    size_t num_samples = chunk_size / (bits_per_sample / 8);
    if (num_samples == 0) {
        std::fclose(file);
        fprintf(stderr, "Error: no audio samples in: %s\n", path.c_str());
        std::exit(1);
    }
    result.samples.resize(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        int16_t sample = 0;
        if (std::fread(&sample, sizeof(int16_t), 1, file) != 1) {
            result.samples.resize(i);
            break;
        }
        result.samples[i] = static_cast<float>(sample) / 32768.0f;
    }
    std::fclose(file);
    result.sample_rate = static_cast<int32_t>(sample_rate);
    return result;
}

static int32_t detect_model_arch(const std::string &model_dir) {
    if (fs::exists(fs::path(model_dir) / "streaming_config.json")) {
        return MOONSHINE_MODEL_ARCH_TINY_STREAMING;
    }
    return MOONSHINE_MODEL_ARCH_TINY;
}

static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s -f <audio_file> -m <model_dir> [options]\n\n"
            "Options:\n"
            "  -f <file>              WAV audio file (required)\n"
            "  -m <model-dir>         Model directory (required)\n"
            "  -a, --model-arch <n>   Model architecture (0=tiny, 1=base, etc.)\n"
            "  -l, --language <lang>  Language (accepted, ignored)\n"
            "  -nt, --no-timestamps   Disable timestamps (default)\n"
            "  -np                    Disable progress (default)\n"
            "  --output-txt           Enable text file output\n"
            "  --output-file <path>   Output file path (writes <path>.txt)\n"
            "  --threads <n>          Number of threads (accepted, ignored)\n"
            "  --help                 Show this help message\n",
            program_name);
}

int main(int argc, char *argv[]) {
    std::string audio_file;
    std::string model_dir;
    int32_t model_arch = -1;
    bool output_txt = false;
    std::string output_file;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-f") {
            if (++i >= argc) {
                fprintf(stderr, "Error: -f requires an argument\n");
                return 1;
            }
            audio_file = argv[i];
        } else if (arg == "-m") {
            if (++i >= argc) {
                fprintf(stderr, "Error: -m requires an argument\n");
                return 1;
            }
            model_dir = argv[i];
        } else if (arg == "-a" || arg == "--model-arch") {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n",
                        arg.c_str());
                return 1;
            }
            model_arch = std::atoi(argv[i]);
        } else if (arg == "-l" || arg == "--language") {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n",
                        arg.c_str());
                return 1;
            }
            // Ignored - language is baked into the model
        } else if (arg == "-nt" || arg == "--no-timestamps") {
            // No-op: timestamps are not printed by default
        } else if (arg == "-np") {
            // No-op: progress is not printed by default
        } else if (arg == "--output-txt") {
            output_txt = true;
        } else if (arg == "--output-file") {
            if (++i >= argc) {
                fprintf(stderr, "Error: --output-file requires an argument\n");
                return 1;
            }
            output_file = argv[i];
        } else if (arg == "--threads") {
            if (++i >= argc) {
                fprintf(stderr, "Error: --threads requires an argument\n");
                return 1;
            }
            // Ignored
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    if (audio_file.empty() || model_dir.empty()) {
        fprintf(stderr, "Error: -f and -m are required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (model_arch < 0) {
        model_arch = detect_model_arch(model_dir);
    }

    WavData wav = load_wav(audio_file);

    int32_t handle = moonshine_load_transcriber_from_files(
        model_dir.c_str(), static_cast<uint32_t>(model_arch), NULL, 0,
        MOONSHINE_HEADER_VERSION);
    if (handle < 0) {
        fprintf(stderr, "Error loading model: %s\n",
                moonshine_error_to_string(handle));
        return 1;
    }

    transcript_t *transcript = NULL;
    int32_t err = moonshine_transcribe_without_streaming(
        handle, wav.samples.data(), wav.samples.size(), wav.sample_rate, 0,
        &transcript);
    if (err != 0) {
        fprintf(stderr, "Error transcribing: %s\n",
                moonshine_error_to_string(err));
        moonshine_free_transcriber(handle);
        return 1;
    }

    // Collect output text
    std::string text;
    for (uint64_t i = 0; i < transcript->line_count; ++i) {
        if (i > 0) text += "\n";
        text += transcript->lines[i].text;
    }

    // Print to stdout
    printf("%s\n", text.c_str());

    // Write to file if requested
    if (output_txt && !output_file.empty()) {
        std::string txt_path = output_file + ".txt";
        std::ofstream ofs(txt_path);
        if (!ofs) {
            fprintf(stderr, "Error: could not write to %s\n",
                    txt_path.c_str());
            moonshine_free_transcriber(handle);
            return 1;
        }
        ofs << text << "\n";
    }

    moonshine_free_transcriber(handle);
    return 0;
}
