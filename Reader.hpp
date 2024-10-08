#ifndef PACKER_H_
#define PACKER_H_


#include <concepts>
#include <fstream>
#include <iterator>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>
#include <ranges>
#include <functional>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdnoreturn.h>
#include <unordered_set>


#ifdef MAPPED
extern uint8_t _binary_resPack_start[], _binary_resPack_end[];
#endif

namespace AssPack {


    #ifdef MAPPED
    #define AP_MODE Unpacker
    auto getResourceFile() -> std::span<uint8_t>
    {
        return {::_binary_resPack_start, ::_binary_resPack_end};
    };
    #else
    #define AP_MODE Tracker
    auto getResourceFile() -> std::span<uint8_t>
    {
        assert(false && "Resource Pack not mapped!");
    }
    #endif



    struct FileInfo {
        std::size_t offset, length;
    };

    class AssetFiles;

    class Tracker
    {
        friend AssetFiles;
        using file_time = std::filesystem::file_time_type;

        std::unordered_map<std::string, file_time> file_tracker;
        const std::filesystem::path asset_path = "assets/";

        Tracker() = default;
        ~Tracker()
        {
            // export Dependency list
            std::cout << "Exporting Resourcepack!" << std::endl;
            std::ofstream out ("dependencies");

            for(auto& [path, _] : file_tracker){
                out << path << std::endl;
            }
        }

    public:
        auto read(const std::string &name) -> std::vector<uint8_t>
        {
            const auto path = asset_path / name;
            if(!file_tracker.contains(name))
                file_tracker[name] = std::filesystem::last_write_time(path);

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            file.unsetf(std::ios::skipws);

            std::vector<uint8_t> res;
            res.reserve(static_cast<std::size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            std::copy(std::istream_iterator<uint8_t>(file),
                    std::istream_iterator<uint8_t>(),
                    std::back_inserter(res));
            return res;
        }

        auto has_changed(const std::string &name)
        {
            assert(file_tracker.contains(name) && "Asset has not been loaded!");
            const file_time new_time = std::filesystem::last_write_time(asset_path / name);
            if(new_time!=file_tracker[name]){
                file_tracker[name]=new_time;
                return true;
            }
            return false;
        }
    };

    class Unpacker
    {
        friend AssetFiles;
        struct info {
            std::size_t offset;
            std::size_t size;
        };
        std::unordered_map<std::string_view, info> header;
        uint8_t *body;

        Unpacker()
        {
            load();
        }

        template<class T>
        static auto read(uint8_t *&ptr) -> T
        {
            T res = *reinterpret_cast<T*>(ptr);
            ptr+=sizeof(T);
            return res;
        }
        static auto read_name(uint8_t *&ptr) -> std::string_view
        {
            auto len = read<uint16_t>(ptr);
            std::string_view name(reinterpret_cast<char*>(ptr), len);
            ptr+=len;
            return name;
        }

        auto load() -> void
        {
            uint8_t *ptr = getResourceFile().data();
            auto entries = read<uint16_t>(ptr);

            while(entries-- > 0) {
                auto name = read_name(ptr);
                auto offset = read<std::size_t>(ptr);
                auto size = read<std::size_t>(ptr);

                header[name] = {offset, size};
            }
            body = ptr;
        }
    public:
        void register_file(const std::string &/* ignore */)
        {}

        auto read(const std::string &name) -> std::vector<uint8_t>
        {
            info info = header[name];
            // std::cout << name << " " << info.offset << " " << info.size << std::endl;
            uint8_t *start = body+info.offset;
            std::vector<uint8_t> v;
            v.assign(start, start+info.size);
            return v;
        }
        auto has_changed(const std::string &name) -> bool
        {
            std::cout << "[WARNING] Request has_changed() on Asset " << name << ", which is in static Storage";
            return false;
        }
    };

    class AssetFiles : public AP_MODE {} asset_files;
}


#endif // PACKER_H_
