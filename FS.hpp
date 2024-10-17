#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <span>

namespace asspack::fs {

    using path = std::filesystem::path;
    using file_time = std::filesystem::file_time_type;

    class FileData;

    class FS
    {
    public:
        struct Info {
            path name;
            std::size_t usage_count {};
            std::vector<uint8_t> data;
        };
        virtual auto load(path) -> FileData = 0;
        virtual auto unload(path) -> void = 0;
    };

    class DependencyTracker : public FS
    {
        const path m_save;
    public:
        DependencyTracker(path save = "")
            : m_save(std::move(save))
        {}
        virtual auto get() const -> std::vector<path> = 0;
        auto save() const -> void;
        ~DependencyTracker();
    };

    class EditTracker
    {
    public:
        virtual auto has_changed(path file) -> bool = 0;
    };

    class Storage : public DependencyTracker, public EditTracker
    {
        struct Info : public FS::Info
        {
            file_time last_change;
        };
        std::unordered_map<path, Info> m_files;

    public:
        auto get() const -> std::vector<path> override;
        auto load(path path) -> FileData override;
        auto unload(path path) -> void override;
        auto has_changed(path file) -> bool override;
    };

    class FileData : public std::span<uint8_t>
    {
        std::shared_ptr<FS> m_parrent;
        FS::Info *m_info;
        FileData(std::shared_ptr<FS> fs, FS::Info *info)
            : std::span<uint8_t>(info->data),
              m_parrent(std::move(fs)),
              m_info(info)
        {
            m_info->usage_count++;
        }
        ~FileData()
        {
            m_info->usage_count--;
            if(m_info->usage_count == 0) m_parrent->unload(std::move(m_info->name));
        }
    };

    class Memory : public FS
    {
        struct Block {
            std::size_t offset;
            std::size_t size;
    };

    protected:
        using Header = std::unordered_map<path, Block>;
        Header m_header;
        std::span<uint8_t> m_data;

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
    public:

        auto load(path file) -> FileData override;
        auto unload(path file) -> void override;

            Memory(std::span<uint8_t> memory) // default ot external _res_bytes[]
                                              // Overwrite in RespackFS with EditTracker
        {
            uint8_t *ptr = memory.data();
            auto entries = read<uint16_t>(ptr);

            while(entries-- > 0) {
                auto name = read_name(ptr);
                auto offset = read<std::size_t>(ptr);
                auto size = read<std::size_t>(ptr);

                m_header[name] = {offset, size};
            }
            m_data = {ptr, memory.end().base()};
        }

    };

    class FileProvider
    {
        // Version Index
    public:
        auto get(path file) -> FileData;
        auto update() -> bool;
    };
}
