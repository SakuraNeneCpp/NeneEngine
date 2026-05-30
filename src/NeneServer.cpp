#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <NeneEngine/NeneServer.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <SDL3/SDL_filesystem.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif

namespace {
std::string nene_save_escape_(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string nene_save_unescape_(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char ch = s[i];
        if (ch != '\\' || i + 1 >= s.size()) {
            out.push_back(ch);
            continue;
        }
        const char next = s[++i];
        switch (next) {
            case '\\': out.push_back('\\'); break;
            case 't': out.push_back('\t'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            default:
                out.push_back('\\');
                out.push_back(next);
                break;
        }
    }
    return out;
}

std::vector<std::string> nene_save_split_tabs_(std::string_view line) {
    std::vector<std::string> out;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            out.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    out.push_back(std::move(current));
    return out;
}

template <class T>
std::string nene_save_to_precise_string_(T value) {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    return oss.str();
}

std::filesystem::path nene_save_slot_path_(const std::string& save_dir,
                                           const std::string& extension,
                                           const std::string& checked_slot_name) {
    std::string file_name = checked_slot_name;
    std::filesystem::path p(file_name);
    if (p.extension().empty()) {
        file_name += extension;
    }
    return std::filesystem::path(save_dir) / file_name;
}

constexpr const char* nene_blackboard_settings_node_ = "@blackboard/settings";
constexpr const char* nene_blackboard_settings_type_ = "NeneBlackboardSettings";
constexpr const char* nene_blackboard_input_map_prefix_ = "@blackboard/input/";
constexpr const char* nene_blackboard_input_map_type_ = "NeneInputMap";

bool nene_blackboard_is_settings_node_(const std::string& path) {
    return path == nene_blackboard_settings_node_;
}

bool nene_blackboard_is_input_map_node_(const std::string& path) {
    return path.rfind(nene_blackboard_input_map_prefix_, 0) == 0;
}

std::string nene_blackboard_input_map_path_(std::string_view map_name) {
    return std::string(nene_blackboard_input_map_prefix_) + std::string(map_name);
}

std::string nene_blackboard_indexed_key_(std::string_view base, int index, std::string_view field) {
    return std::string(base) + "." + std::to_string(index) + "." + std::string(field);
}

bool nene_blackboard_valid_device_(int value) {
    switch (static_cast<NeneInputDevice>(value)) {
        case NeneInputDevice::Unknown:
        case NeneInputDevice::Keyboard:
        case NeneInputDevice::Mouse:
        case NeneInputDevice::Gamepad:
            return true;
    }
    return false;
}

bool nene_blackboard_valid_control_(int value) {
    switch (static_cast<NeneInputControl>(value)) {
        case NeneInputControl::Button:
        case NeneInputControl::Axis:
            return true;
    }
    return false;
}

bool nene_sound_has_extension_(std::string_view path, std::string_view extension) {
    if (path.size() < extension.size()) return false;
    const std::string_view tail = path.substr(path.size() - extension.size());
    for (std::size_t i = 0; i < tail.size(); ++i) {
        const auto a = static_cast<unsigned char>(tail[i]);
        const auto b = static_cast<unsigned char>(extension[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

#ifdef _WIN32
struct NeneDecodedPcm_ {
    SDL_AudioSpec spec{};
    std::vector<std::uint8_t> data;
};

std::string nene_sound_hresult_(HRESULT hr) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return os.str();
}

void nene_sound_check_hr_(HRESULT hr, std::string_view where) {
    if (SUCCEEDED(hr)) return;
    throw std::runtime_error(std::string(where) + " failed: " + nene_sound_hresult_(hr));
}

std::wstring nene_sound_utf8_to_wide_(const std::string& text) {
    if (text.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                     text.c_str(), -1, nullptr, 0);
    if (needed <= 0) {
        needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    }
    if (needed <= 0) {
        throw std::runtime_error("failed to convert UTF-8 path to UTF-16");
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), needed);
    out.resize(static_cast<std::size_t>(needed - 1));
    return out;
}

NeneDecodedPcm_ nene_sound_decode_media_foundation_(const std::string& path) {
    using Microsoft::WRL::ComPtr;

    const std::wstring wide_path = nene_sound_utf8_to_wide_(path);
    ComPtr<IMFSourceReader> reader;
    nene_sound_check_hr_(
        MFCreateSourceReaderFromURL(wide_path.c_str(), nullptr, reader.GetAddressOf()),
        "MFCreateSourceReaderFromURL");

    nene_sound_check_hr_(
        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE),
        "IMFSourceReader::SetStreamSelection(all)");
    nene_sound_check_hr_(
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE),
        "IMFSourceReader::SetStreamSelection(audio)");

    ComPtr<IMFMediaType> output_type;
    nene_sound_check_hr_(MFCreateMediaType(output_type.GetAddressOf()), "MFCreateMediaType");
    nene_sound_check_hr_(
        output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
        "IMFMediaType::SetGUID(MF_MT_MAJOR_TYPE)");
    nene_sound_check_hr_(
        output_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
        "IMFMediaType::SetGUID(MF_MT_SUBTYPE)");
    nene_sound_check_hr_(
        output_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
        "IMFMediaType::SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE)");
    nene_sound_check_hr_(
        reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, output_type.Get()),
        "IMFSourceReader::SetCurrentMediaType");

    ComPtr<IMFMediaType> actual_type;
    nene_sound_check_hr_(
        reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, actual_type.GetAddressOf()),
        "IMFSourceReader::GetCurrentMediaType");

    UINT32 channels = 0;
    UINT32 frequency = 0;
    UINT32 bits_per_sample = 16;
    nene_sound_check_hr_(
        actual_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels),
        "IMFMediaType::GetUINT32(MF_MT_AUDIO_NUM_CHANNELS)");
    nene_sound_check_hr_(
        actual_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &frequency),
        "IMFMediaType::GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND)");
    actual_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
    if (channels == 0 || frequency == 0 || bits_per_sample != 16) {
        throw std::runtime_error("Media Foundation returned an unsupported PCM format");
    }

    NeneDecodedPcm_ decoded;
    decoded.spec.format = SDL_AUDIO_S16;
    decoded.spec.channels = static_cast<int>(channels);
    decoded.spec.freq = static_cast<int>(frequency);

    for (;;) {
        DWORD stream_index = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        nene_sound_check_hr_(
            reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                               &stream_index, &flags, &timestamp, sample.GetAddressOf()),
            "IMFSourceReader::ReadSample");
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buffer;
        nene_sound_check_hr_(
            sample->ConvertToContiguousBuffer(buffer.GetAddressOf()),
            "IMFSample::ConvertToContiguousBuffer");

        BYTE* audio_data = nullptr;
        DWORD max_length = 0;
        DWORD current_length = 0;
        nene_sound_check_hr_(
            buffer->Lock(&audio_data, &max_length, &current_length),
            "IMFMediaBuffer::Lock");
        if (audio_data && current_length > 0) {
            const auto* begin = reinterpret_cast<const std::uint8_t*>(audio_data);
            decoded.data.insert(decoded.data.end(), begin, begin + current_length);
        }
        buffer->Unlock();
    }

    if (decoded.data.empty()) {
        throw std::runtime_error("Media Foundation decoded no audio data");
    }
    return decoded;
}
#endif
}

// NeneSaveWriter
NeneSaveWriter::NeneSaveWriter(NeneSaveNodeRecord& record)
    : record_(&record) {}

void NeneSaveWriter::set(std::string key, std::string value) {
    if (!record_) return;
    record_->values[std::move(key)] = std::move(value);
}

void NeneSaveWriter::set(std::string key, const char* value) {
    set(std::move(key), value ? std::string(value) : std::string());
}

void NeneSaveWriter::set_bool(std::string key, bool value) {
    set(std::move(key), value ? "true" : "false");
}

void NeneSaveWriter::set_int(std::string key, int value) {
    set(std::move(key), std::to_string(value));
}

void NeneSaveWriter::set_uint(std::string key, std::uint32_t value) {
    set(std::move(key), std::to_string(value));
}

void NeneSaveWriter::set_float(std::string key, float value) {
    set(std::move(key), nene_save_to_precise_string_(value));
}

void NeneSaveWriter::set_double(std::string key, double value) {
    set(std::move(key), nene_save_to_precise_string_(value));
}

// NeneSaveReader
NeneSaveReader::NeneSaveReader(const NeneSaveNodeRecord& record)
    : record_(&record) {}

bool NeneSaveReader::has(std::string_view key) const {
    if (!record_) return false;
    return record_->values.find(std::string(key)) != record_->values.end();
}

std::string NeneSaveReader::get_string(std::string_view key, std::string default_value) const {
    if (!record_) return default_value;
    const auto it = record_->values.find(std::string(key));
    return (it == record_->values.end()) ? default_value : it->second;
}

bool NeneSaveReader::get_bool(std::string_view key, bool default_value) const {
    const std::string value = get_string(key, default_value ? "true" : "false");
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    return default_value;
}

int NeneSaveReader::get_int(std::string_view key, int default_value) const {
    try {
        return std::stoi(get_string(key, std::to_string(default_value)));
    } catch (...) {
        return default_value;
    }
}

std::uint32_t NeneSaveReader::get_uint(std::string_view key, std::uint32_t default_value) const {
    try {
        return static_cast<std::uint32_t>(std::stoul(get_string(key, std::to_string(default_value))));
    } catch (...) {
        return default_value;
    }
}

float NeneSaveReader::get_float(std::string_view key, float default_value) const {
    try {
        return std::stof(get_string(key, nene_save_to_precise_string_(default_value)));
    } catch (...) {
        return default_value;
    }
}

double NeneSaveReader::get_double(std::string_view key, double default_value) const {
    try {
        return std::stod(get_string(key, nene_save_to_precise_string_(default_value)));
    } catch (...) {
        return default_value;
    }
}

// NeneSaveDocument
void NeneSaveDocument::clear() {
    format_version = current_format_version;
    metadata.clear();
    nodes.clear();
}

void NeneSaveDocument::set_metadata(std::string key, std::string value) {
    metadata[std::move(key)] = std::move(value);
}

bool NeneSaveDocument::has_metadata(std::string_view key) const {
    return metadata.find(std::string(key)) != metadata.end();
}

std::string NeneSaveDocument::get_metadata(std::string_view key, std::string default_value) const {
    const auto it = metadata.find(std::string(key));
    return (it == metadata.end()) ? default_value : it->second;
}

NeneSaveNodeRecord& NeneSaveDocument::node(std::string path) {
    return nodes[std::move(path)];
}

const NeneSaveNodeRecord* NeneSaveDocument::find_node(std::string_view path) const {
    const auto it = nodes.find(std::string(path));
    return (it == nodes.end()) ? nullptr : &it->second;
}

bool NeneSaveDocument::has_node(std::string_view path) const {
    return find_node(path) != nullptr;
}

std::string NeneSaveDocument::serialize() const {
    std::ostringstream out;
    out << "NENE_SAVE\t" << format_version << "\n";
    for (const auto& [key, value] : metadata) {
        out << "meta\t"
            << nene_save_escape_(key) << "\t"
            << nene_save_escape_(value) << "\n";
    }
    for (const auto& [path, record] : nodes) {
        out << "node\t"
            << nene_save_escape_(path) << "\t"
            << nene_save_escape_(record.type) << "\n";
        for (const auto& [key, value] : record.values) {
            out << "value\t"
                << nene_save_escape_(path) << "\t"
                << nene_save_escape_(key) << "\t"
                << nene_save_escape_(value) << "\n";
        }
    }
    return out.str();
}

NeneSaveDocument NeneSaveDocument::parse(std::string_view text) {
    NeneSaveDocument doc;
    doc.clear();
    std::istringstream in{std::string(text)};
    std::string line;
    std::size_t line_no = 0;
    bool header_seen = false;

    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        auto parts = nene_save_split_tabs_(line);
        if (!header_seen) {
            if (parts.size() != 2 || parts[0] != "NENE_SAVE") {
                throw std::runtime_error("NeneSaveDocument: invalid header");
            }
            try {
                doc.format_version = static_cast<std::uint32_t>(std::stoul(parts[1]));
            } catch (...) {
                throw std::runtime_error("NeneSaveDocument: invalid format version");
            }
            header_seen = true;
            continue;
        }

        if (parts[0] == "meta") {
            if (parts.size() != 3) {
                throw std::runtime_error("NeneSaveDocument: invalid meta line " + std::to_string(line_no));
            }
            doc.metadata[nene_save_unescape_(parts[1])] = nene_save_unescape_(parts[2]);
            continue;
        }

        if (parts[0] == "node") {
            if (parts.size() != 3) {
                throw std::runtime_error("NeneSaveDocument: invalid node line " + std::to_string(line_no));
            }
            auto& record = doc.nodes[nene_save_unescape_(parts[1])];
            record.type = nene_save_unescape_(parts[2]);
            continue;
        }

        if (parts[0] == "value") {
            if (parts.size() != 4) {
                throw std::runtime_error("NeneSaveDocument: invalid value line " + std::to_string(line_no));
            }
            auto& record = doc.nodes[nene_save_unescape_(parts[1])];
            record.values[nene_save_unescape_(parts[2])] = nene_save_unescape_(parts[3]);
            continue;
        }

        throw std::runtime_error("NeneSaveDocument: unknown line kind " + std::to_string(line_no));
    }

    if (!header_seen) {
        throw std::runtime_error("NeneSaveDocument: empty document");
    }
    return doc;
}

// NeneBlackboard
void NeneBlackboard::save_settings(NeneSaveDocument& doc) const {
    doc.set_metadata("blackboard_settings_version", "1");

    for (auto it = doc.nodes.begin(); it != doc.nodes.end();) {
        if (nene_blackboard_is_settings_node_(it->first)
            || nene_blackboard_is_input_map_node_(it->first)) {
            it = doc.nodes.erase(it);
        } else {
            ++it;
        }
    }

    auto& settings_record = doc.node(nene_blackboard_settings_node_);
    settings_record.type = nene_blackboard_settings_type_;
    NeneSaveWriter settings(settings_record);
    settings.set_int("fps", fps);
    settings.set_int("window_x", window_x);
    settings.set_int("window_y", window_y);
    settings.set_int("window_w", window_w);
    settings.set_int("window_h", window_h);

    std::vector<std::string> float_keys;
    float_keys.reserve(persistent_float_keys_.size());
    for (const auto& key : persistent_float_keys_) {
        if (user_floats.find(key) != user_floats.end()) float_keys.push_back(key);
    }
    std::sort(float_keys.begin(), float_keys.end());

    int saved_float_count = 0;
    for (const auto& key : float_keys) {
        const auto it = user_floats.find(key);
        if (it == user_floats.end()) continue;
        settings.set(nene_blackboard_indexed_key_("user_float", saved_float_count, "key"), key);
        settings.set_float(nene_blackboard_indexed_key_("user_float", saved_float_count, "value"), it->second);
        ++saved_float_count;
    }
    settings.set_int("user_float_count", saved_float_count);

    std::vector<std::string> map_names;
    map_names.reserve(input_maps.size());
    for (const auto& [map_name, bindings] : input_maps) {
        map_names.push_back(map_name);
    }
    std::sort(map_names.begin(), map_names.end());

    for (const auto& map_name : map_names) {
        const auto map_it = input_maps.find(map_name);
        if (map_it == input_maps.end()) continue;

        auto& map_record = doc.node(nene_blackboard_input_map_path_(map_name));
        map_record.type = nene_blackboard_input_map_type_;
        NeneSaveWriter map_writer(map_record);
        map_writer.set("name", map_name);
        map_writer.set_int("count", static_cast<int>(map_it->second.size()));

        for (int i = 0; i < static_cast<int>(map_it->second.size()); ++i) {
            const auto& binding = map_it->second[static_cast<std::size_t>(i)];
            map_writer.set_int(nene_blackboard_indexed_key_("binding", i, "device"),
                               static_cast<int>(binding.device));
            map_writer.set_int(nene_blackboard_indexed_key_("binding", i, "control"),
                               static_cast<int>(binding.control));
            map_writer.set_int(nene_blackboard_indexed_key_("binding", i, "code"), binding.code);
            map_writer.set(nene_blackboard_indexed_key_("binding", i, "action"), binding.action);
            map_writer.set_float(nene_blackboard_indexed_key_("binding", i, "scale"), binding.scale);
            map_writer.set_int(nene_blackboard_indexed_key_("binding", i, "player"), binding.player);
            map_writer.set_float(nene_blackboard_indexed_key_("binding", i, "dead_zone"), binding.dead_zone);
        }
    }
}

void NeneBlackboard::load_settings(const NeneSaveDocument& doc) {
    if (const auto* settings_record = doc.find_node(nene_blackboard_settings_node_)) {
        NeneSaveReader settings(*settings_record);
        const int loaded_fps = settings.get_int("fps", fps);
        if (loaded_fps > 0) fps = loaded_fps;

        window_x = settings.get_int("window_x", window_x);
        window_y = settings.get_int("window_y", window_y);
        const int loaded_window_w = settings.get_int("window_w", window_w);
        const int loaded_window_h = settings.get_int("window_h", window_h);
        if (loaded_window_w > 0) window_w = loaded_window_w;
        if (loaded_window_h > 0) window_h = loaded_window_h;
        ground_y = static_cast<float>(window_h) - 120.0f;

        const int user_float_count = settings.get_int("user_float_count", 0);
        for (int i = 0; i < user_float_count; ++i) {
            const std::string key =
                settings.get_string(nene_blackboard_indexed_key_("user_float", i, "key"));
            if (key.empty()) continue;
            const float value =
                settings.get_float(nene_blackboard_indexed_key_("user_float", i, "value"), 0.0f);
            user_floats[key] = value;
            persistent_float_keys_.insert(key);
        }
    }

    for (const auto& [path, record] : doc.nodes) {
        if (!nene_blackboard_is_input_map_node_(path)) continue;

        NeneSaveReader reader(record);
        const std::string fallback_name =
            path.substr(std::string(nene_blackboard_input_map_prefix_).size());
        const std::string map_name = reader.get_string("name", fallback_name);
        if (map_name.empty()) continue;

        int count = reader.get_int("count", 0);
        if (count < 0) count = 0;

        std::vector<NeneInputBinding> bindings;
        bindings.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            const int device_value = reader.get_int(
                nene_blackboard_indexed_key_("binding", i, "device"),
                static_cast<int>(NeneInputDevice::Keyboard));
            const int control_value = reader.get_int(
                nene_blackboard_indexed_key_("binding", i, "control"),
                static_cast<int>(NeneInputControl::Button));
            if (!nene_blackboard_valid_device_(device_value)
                || !nene_blackboard_valid_control_(control_value)) {
                continue;
            }

            const std::string action =
                reader.get_string(nene_blackboard_indexed_key_("binding", i, "action"));
            if (action.empty()) continue;

            bindings.emplace_back(
                static_cast<NeneInputDevice>(device_value),
                static_cast<NeneInputControl>(control_value),
                reader.get_int(nene_blackboard_indexed_key_("binding", i, "code"), 0),
                action,
                reader.get_float(nene_blackboard_indexed_key_("binding", i, "scale"), 1.0f),
                reader.get_int(nene_blackboard_indexed_key_("binding", i, "player"), 0),
                reader.get_float(nene_blackboard_indexed_key_("binding", i, "dead_zone"), 0.35f));
        }

        input_maps[map_name] = std::move(bindings);
    }
}

// NeneSaveService
NeneSaveService::NeneSaveService(std::string org, std::string app,
                                 std::string save_dir, std::string extension)
    : extension_(std::move(extension)) {
    if (org.empty()) throw std::runtime_error("NeneSaveService: org is empty");
    if (app.empty()) throw std::runtime_error("NeneSaveService: app is empty");
    if (save_dir.empty()) throw std::runtime_error("NeneSaveService: save_dir is empty");
    if (extension_.empty() || extension_[0] != '.') {
        throw std::runtime_error("NeneSaveService: extension must start with '.'");
    }

    char* pref = SDL_GetPrefPath(org.c_str(), app.c_str());
    if (!pref) {
        throw std::runtime_error(std::string("NeneSaveService: SDL_GetPrefPath failed: ") + SDL_GetError());
    }
    base_path_ = pref;
    SDL_free(pref);

    const std::filesystem::path save_dir_path = std::filesystem::path(base_path_) / save_dir;
    std::filesystem::create_directories(save_dir_path);
    save_dir_path_ = save_dir_path.string();
}

std::string NeneSaveService::checked_slot_name_(std::string_view slot_name) {
    if (slot_name.empty()) throw std::runtime_error("NeneSaveService: slot_name is empty");
    std::string out(slot_name);
    if (out.find('/') != std::string::npos || out.find('\\') != std::string::npos
        || out.find(':') != std::string::npos) {
        throw std::runtime_error("NeneSaveService: slot_name must not contain path separators");
    }
    return out;
}

std::string NeneSaveService::slot_path(std::string_view slot_name) const {
    return nene_save_slot_path_(save_dir_path_, extension_, checked_slot_name_(slot_name)).string();
}

bool NeneSaveService::slot_exists(std::string_view slot_name) const {
    return std::filesystem::exists(
        nene_save_slot_path_(save_dir_path_, extension_, checked_slot_name_(slot_name)));
}

void NeneSaveService::save(std::string_view slot_name, const NeneSaveDocument& doc) const {
    const std::filesystem::path target =
        nene_save_slot_path_(save_dir_path_, extension_, checked_slot_name_(slot_name));
    std::filesystem::create_directories(target.parent_path());
    const std::filesystem::path tmp = target.string() + ".tmp";

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("NeneSaveService: failed to open temp save file");
        const std::string text = doc.serialize();
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) throw std::runtime_error("NeneSaveService: failed to write save file");
    }

    const std::filesystem::path backup = target.string() + ".bak";
    std::error_code ec;
    std::filesystem::remove(backup, ec);
    ec.clear();
    if (std::filesystem::exists(target)) {
        std::filesystem::rename(target, backup, ec);
        if (ec) {
            std::filesystem::remove(tmp);
            throw std::runtime_error("NeneSaveService: failed to backup old save file: " + ec.message());
        }
    }

    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        std::error_code restore_ec;
        if (std::filesystem::exists(backup)) {
            std::filesystem::rename(backup, target, restore_ec);
        }
        std::filesystem::remove(tmp);
        throw std::runtime_error("NeneSaveService: failed to replace save file: " + ec.message());
    }
    std::filesystem::remove(backup, ec);
}

NeneSaveDocument NeneSaveService::load(std::string_view slot_name) const {
    const std::filesystem::path target =
        nene_save_slot_path_(save_dir_path_, extension_, checked_slot_name_(slot_name));
    std::ifstream in(target, std::ios::binary);
    if (!in) throw std::runtime_error("NeneSaveService: failed to open save file");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return NeneSaveDocument::parse(buffer.str());
}

bool NeneSaveService::remove(std::string_view slot_name) const {
    std::error_code ec;
    return std::filesystem::remove(
        nene_save_slot_path_(save_dir_path_, extension_, checked_slot_name_(slot_name)), ec);
}

std::vector<std::string> NeneSaveService::list_slots() const {
    std::vector<std::string> out;
    const std::filesystem::path dir(save_dir_path_);
    if (!std::filesystem::exists(dir)) return out;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto path = entry.path();
        if (path.extension().string() != extension_) continue;
        out.push_back(path.stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

NeneFontLoader::NeneFontLoader(SDL_Renderer* renderer)
    : renderer_(renderer) {
    if (!renderer_) {
        throw std::runtime_error("NeneFontLoader: renderer is null");
    }
    if (!TTF_Init()) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + SDL_GetError());
    }
}

NeneFontLoader::~NeneFontLoader() {
    for (auto& [key, font] : fontCache_) {
        if (font) TTF_CloseFont(font);
    }
    fontCache_.clear();

    for (auto& [fontKey, tex] : textCache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    textCache_.clear();

    TTF_Quit();
}

TTF_Font* NeneFontLoader::get_font(const std::string& fontPath, int fontSize) {
    std::string key = fontPath + "#" + std::to_string(fontSize);
    auto it = fontCache_.find(key);
    if (it != fontCache_.end()) return it->second;

    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!font) {
        throw std::runtime_error(std::string("TTF_OpenFont failed: ") + SDL_GetError());
    }
    fontCache_[key] = font;
    return font;
}

SDL_Texture* NeneFontLoader::get_text_texture(const std::string& fontPath, int fontSize,
                                         const std::string& text, SDL_Color color) {
    FontKey fk{ text, fontSize, color };
    auto it = textCache_.find(fk);
    if (it != textCache_.end()) return it->second;

    TTF_Font* font = get_font(fontPath, fontSize);

    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surf) {
        throw std::runtime_error(std::string("TTF_RenderText_Blended failed: ") + SDL_GetError());
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_DestroySurface(surf);

    if (!tex) {
        throw std::runtime_error(std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError());
    }

    textCache_[fk] = tex;
    return tex;
}

NeneSoundLoader::NeneSoundLoader(int se_track_count) {
    auto cleanup = [this]() {
        for (auto* track : se_tracks_) {
            if (track) MIX_DestroyTrack(track);
        }
        se_tracks_.clear();
        se_track_gains_.clear();
        if (bgm_track_) {
            MIX_DestroyTrack(bgm_track_);
            bgm_track_ = nullptr;
        }
        for (auto& [path, audio] : cache_) {
            if (audio) MIX_DestroyAudio(audio);
        }
        cache_.clear();
        if (mixer_) {
            MIX_DestroyMixer(mixer_);
            mixer_ = nullptr;
        }
        if (mixer_initialized_) {
            MIX_Quit();
            mixer_initialized_ = false;
        }
#ifdef _WIN32
        if (media_foundation_started_) {
            MFShutdown();
            media_foundation_started_ = false;
        }
        if (com_initialized_) {
            CoUninitialize();
            com_initialized_ = false;
        }
#endif
    };

    try {
#ifdef _WIN32
        const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(co_hr)) {
            com_initialized_ = true;
        } else if (co_hr != RPC_E_CHANGED_MODE) {
            nene_sound_check_hr_(co_hr, "CoInitializeEx");
        }
        nene_sound_check_hr_(MFStartup(MF_VERSION), "MFStartup");
        media_foundation_started_ = true;
#endif

        if (!MIX_Init()) {
            throw std::runtime_error(std::string("MIX_Init failed: ") + SDL_GetError());
        }
        mixer_initialized_ = true;

        mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!mixer_) {
            throw std::runtime_error(std::string("MIX_CreateMixerDevice failed: ") + SDL_GetError());
        }

        const int count = std::max(1, se_track_count);
        se_tracks_.reserve(static_cast<std::size_t>(count));
        se_track_gains_.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            MIX_Track* track = MIX_CreateTrack(mixer_);
            if (!track) {
                throw std::runtime_error(std::string("MIX_CreateTrack(se) failed: ") + SDL_GetError());
            }
            MIX_TagTrack(track, "se");
            se_tracks_.push_back(track);
            se_track_gains_.push_back(1.0f);
        }

        bgm_track_ = MIX_CreateTrack(mixer_);
        if (!bgm_track_) {
            throw std::runtime_error(std::string("MIX_CreateTrack(bgm) failed: ") + SDL_GetError());
        }
        MIX_TagTrack(bgm_track_, "bgm");
    } catch (...) {
        cleanup();
        throw;
    }
}

NeneSoundLoader::~NeneSoundLoader() {
    stop_bgm();
    for (auto* track : se_tracks_) {
        if (track) MIX_DestroyTrack(track);
    }
    se_tracks_.clear();
    se_track_gains_.clear();

    if (bgm_track_) {
        MIX_DestroyTrack(bgm_track_);
        bgm_track_ = nullptr;
    }

    for (auto& [path, audio] : cache_) {
        if (audio) MIX_DestroyAudio(audio);
    }
    cache_.clear();

    if (mixer_) {
        MIX_DestroyMixer(mixer_);
        mixer_ = nullptr;
    }
    if (mixer_initialized_) {
        MIX_Quit();
        mixer_initialized_ = false;
    }
#ifdef _WIN32
    if (media_foundation_started_) {
        MFShutdown();
        media_foundation_started_ = false;
    }
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
#endif
}

void NeneSoundLoader::preload(const std::string& path) {
    (void)get_audio_(path);
}

void NeneSoundLoader::play_se(const std::string& path, float gain) {
    const float base_gain = clamp_gain_(gain);
    const float effective_gain = se_volume_ * base_gain;
    if (effective_gain <= 0.0f) return;

    MIX_Audio* audio = get_audio_(path);
    MIX_Track* track = acquire_se_track_();
    if (!MIX_SetTrackAudio(track, audio)) {
        throw std::runtime_error(std::string("MIX_SetTrackAudio(se) failed: ") + SDL_GetError());
    }

    for (std::size_t i = 0; i < se_tracks_.size(); ++i) {
        if (se_tracks_[i] == track) {
            se_track_gains_[i] = base_gain;
            break;
        }
    }

    MIX_SetTrackGain(track, effective_gain);
    if (!MIX_PlayTrack(track, 0)) {
        throw std::runtime_error(std::string("MIX_PlayTrack(se) failed: ") + SDL_GetError());
    }
}

void NeneSoundLoader::play_bgm(const std::string& path, float gain) {
    if (!bgm_track_) return;
    bgm_gain_ = clamp_gain_(gain);

    if (bgm_path_ == path && MIX_TrackPlaying(bgm_track_)) {
        MIX_SetTrackGain(bgm_track_, bgm_volume_ * bgm_gain_);
        return;
    }

    MIX_Audio* audio = get_audio_(path);
    MIX_StopTrack(bgm_track_, 0);
    if (!MIX_SetTrackAudio(bgm_track_, audio)) {
        throw std::runtime_error(std::string("MIX_SetTrackAudio(bgm) failed: ") + SDL_GetError());
    }
    MIX_SetTrackGain(bgm_track_, bgm_volume_ * bgm_gain_);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        throw std::runtime_error(std::string("SDL_CreateProperties failed: ") + SDL_GetError());
    }
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    const bool ok = MIX_PlayTrack(bgm_track_, props);
    SDL_DestroyProperties(props);
    if (!ok) {
        throw std::runtime_error(std::string("MIX_PlayTrack(bgm) failed: ") + SDL_GetError());
    }
    bgm_path_ = path;
}

void NeneSoundLoader::stop_bgm() {
    if (!bgm_track_) return;
    MIX_StopTrack(bgm_track_, 0);
    MIX_SetTrackAudio(bgm_track_, nullptr);
    bgm_path_.clear();
}

void NeneSoundLoader::set_se_volume(float volume) {
    se_volume_ = clamp_gain_(volume);
    for (std::size_t i = 0; i < se_tracks_.size(); ++i) {
        if (se_tracks_[i]) {
            MIX_SetTrackGain(se_tracks_[i], se_volume_ * se_track_gains_[i]);
        }
    }
}

void NeneSoundLoader::set_bgm_volume(float volume) {
    bgm_volume_ = clamp_gain_(volume);
    if (bgm_track_) {
        MIX_SetTrackGain(bgm_track_, bgm_volume_ * bgm_gain_);
    }
}

MIX_Audio* NeneSoundLoader::get_audio_(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;
    MIX_Audio* audio = load_audio_(path);
    cache_.emplace(path, audio);
    return audio;
}

MIX_Audio* NeneSoundLoader::load_audio_(const std::string& path) {
    if (!mixer_) {
        throw std::runtime_error("NeneSoundLoader: mixer is not initialized");
    }

    if (MIX_Audio* audio = MIX_LoadAudio(mixer_, path.c_str(), false)) {
        return audio;
    }

    const std::string mixer_error = SDL_GetError();
#ifdef _WIN32
    if (nene_sound_has_extension_(path, ".mp3")) {
        try {
            const NeneDecodedPcm_ decoded = nene_sound_decode_media_foundation_(path);
            MIX_Audio* audio = MIX_LoadRawAudio(mixer_, decoded.data.data(),
                                                decoded.data.size(), &decoded.spec);
            if (!audio) {
                throw std::runtime_error(std::string("MIX_LoadRawAudio failed: ") + SDL_GetError());
            }
            return audio;
        } catch (const std::exception& e) {
            throw std::runtime_error("NeneSoundLoader: failed to load '" + path
                                     + "': " + mixer_error
                                     + "; Media Foundation fallback: " + e.what());
        }
    }
#endif

    throw std::runtime_error("NeneSoundLoader: MIX_LoadAudio failed '" + path
                             + "': " + mixer_error);
}

MIX_Track* NeneSoundLoader::acquire_se_track_() {
    if (se_tracks_.empty()) {
        throw std::runtime_error("NeneSoundLoader: no SE tracks available");
    }
    for (auto* track : se_tracks_) {
        if (track && !MIX_TrackPlaying(track)) return track;
    }
    MIX_Track* track = se_tracks_[next_se_track_ % se_tracks_.size()];
    next_se_track_ = (next_se_track_ + 1) % se_tracks_.size();
    MIX_StopTrack(track, 0);
    return track;
}

float NeneSoundLoader::clamp_gain_(float gain) {
    if (!std::isfinite(gain)) return 0.0f;
    return std::clamp(gain, 0.0f, 1.0f);
}
