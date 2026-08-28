#include "deimos/data_tables.hpp"
#include "deimos/film.hpp"
#include "deimos/legacy_text.hpp"
#include "deimos/level.hpp"
#include "deimos/pak_archive.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: deimos_reference_probe /path/to/Game.pak\n";
        return 2;
    }

    std::string error;
    auto pak = deimos::PakArchive::open(std::filesystem::path(argv[1]), &error);
    if (!pak) {
        std::cerr << error << '\n';
        return 3;
    }

    std::size_t files = 0, levels = 0, objects = 0, films = 0;
    std::size_t id_lists = 0, float_lists = 0, color_lists = 0;
    std::size_t text_formats = 0, string_lists = 0, rect_lists = 0;

    for (const auto& entry : pak->entries()) {
        if (entry.is_directory) continue;
        ++files;
        auto bytes = pak->read(entry, &error); // also CRC-validates
        if (!bytes) {
            std::cerr << entry.path << ": " << error << '\n';
            return 4;
        }

        const auto dot = entry.path.find_last_of('.');
        if (dot == std::string::npos) continue;
        const auto ext = entry.path.substr(dot);

        if (ext == ".leve") {
            auto level = deimos::decode_and_parse_level(*bytes, &error);
            if (!level) { std::cerr << entry.path << ": " << error << '\n'; return 5; }
            ++levels;
            objects += level->objects.size();
        } else if (ext == ".film") {
            if (!deimos::parse_film_v10005(*bytes, &error)) {
                std::cerr << entry.path << ": " << error << '\n'; return 6;
            }
            ++films;
        } else if (ext == ".idli" || ext == ".flli" || ext == ".coli" || ext == ".tefo" ||
                   ext == ".stli" || ext == ".reli") {
            auto doc = deimos::parse_tagged_text(deimos::decode_legacy_text(*bytes), &error);
            if (!doc) { std::cerr << entry.path << ": " << error << '\n'; return 7; }
            bool ok = false;
            if (ext == ".idli") { ok = bool(deimos::parse_id_list(*doc, &error)); ++id_lists; }
            if (ext == ".flli") { ok = bool(deimos::parse_float_list(*doc, &error)); ++float_lists; }
            if (ext == ".coli") { ok = bool(deimos::parse_color_list(*doc, &error)); ++color_lists; }
            if (ext == ".tefo") { ok = bool(deimos::parse_text_format(*doc, &error)); ++text_formats; }
            if (ext == ".stli") { ok = bool(deimos::parse_string_list(*doc, &error)); ++string_lists; }
            if (ext == ".reli") { ok = bool(deimos::parse_rect_list(*doc, &error)); ++rect_lists; }
            if (!ok) { std::cerr << entry.path << ": " << error << '\n'; return 8; }
        }
    }

    std::cout << "Game.pak clean-core validation PASS\n"
              << "  actual files: " << files << '\n'
              << "  levels: " << levels << '\n'
              << "  placed objects: " << objects << '\n'
              << "  films: " << films << '\n'
              << "  ID lists: " << id_lists << '\n'
              << "  float lists: " << float_lists << '\n'
              << "  color lists: " << color_lists << '\n'
              << "  text formats: " << text_formats << '\n'
              << "  string lists: " << string_lists << '\n'
              << "  rect lists: " << rect_lists << '\n';
    return 0;
}
