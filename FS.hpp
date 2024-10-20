#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <type_traits>
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
            std::vector<uint8_t> data;
        };
        virtual auto load(const path &) -> std::span<uint8_t> = 0;
        virtual auto contains(const path &) -> bool = 0;
        virtual auto unload(const path &) -> void = 0;
    };

    class EditTracker
    {
    public:
        virtual auto has_changed(const path &file) -> bool = 0;
    };

    class Storage : public FS, public EditTracker
    {
        struct Info : public FS::Info
        {
            file_time last_change;
        };
        std::unordered_map<path, Info> m_files;
        path m_directory;

    public:
        Storage(path directory="") : m_directory(std::move(directory))
        {}

        auto load(const path &file) -> std::span<uint8_t> override // TODO atomic usage counter
        {
            m_files[file] = {{read(m_directory/file)}, std::filesystem::last_write_time(file)};
            return m_files[file].data;
        }
        auto unload(const path &file) -> void override
        {
            m_files.erase(file);
        }
        auto has_changed(const path &file) -> bool override
        {
            return std::filesystem::last_write_time(file)==m_files[file].last_change;
        }
        auto contains(const path &file) -> bool override
        {
            return std::filesystem::exists(file);
        }
        static auto read(const path &file) -> std::vector<uint8_t>;
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
        std::optional<std::vector<uint8_t>> m_own {};
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

        Memory(const path &file)
            : m_own(Storage::read(file))
        {
            from_span(*m_own);
        }
        Memory(std::span<uint8_t> data)
        {
            from_span(data);
        }
        auto from_span(std::span<uint8_t> data) ->void
        {
            uint8_t *ptr = data.data();
            auto entries = read<uint16_t>(ptr);

            while(entries-- > 0) {
                auto name = read_name(ptr);
                auto offset = read<std::size_t>(ptr);
                auto size = read<std::size_t>(ptr);

                m_header[name] = {offset, size};
            }
            m_data = {ptr, m_data.end().base()};
        }

        auto load(const path &file) -> std::span<uint8_t> override
        {
            Block b = m_header[file];
            return m_data.subspan(b.offset, b.size);
        }
        auto unload(const path & /* ignore */) -> void override
        {}
        auto contains(const path &file) -> bool override
        {
            return m_header.contains(file);
        }
    };

    class AssetFiles : public FS, public EditTracker
    {
        // maybe setting to keep initial FS from cache
        struct FileInfo {
            FS *source;
        };
        struct FSInfo {
            std::string name;
            std::unique_ptr<FS> fs;
        };
        std::unordered_map<path, FileInfo> m_files;
        std::list<FSInfo> m_sources;
    public:
        template<class F>
            requires std::is_base_of_v<FS, F>
        auto add_top(std::string name, std::unique_ptr<F> &&fs) -> void
        {
            m_sources.emplace_back(std::move(name), std::move(fs));
        }
        auto add_bottom(std::string name, std::unique_ptr<F> &&fs) -> void
        {
            m_sources.emplace_front(std::move(name), std::move(fs));
        }
        auto remove(const std::string &name) -> void
        {
            auto it = std::ranges::find_if(m_sources, [&name](FSInfo &info){return info.name==name;});
            if(it==m_sources.end()) return;

            // free all files associatet with FS(name)
            std::erase_if(m_files, [&it](const FileInfo &info){return info.source==it->fs.get();});
            m_sources.erase(it);
        }

        auto load(const path &file) -> std::span<uint8_t> override
        {
            auto it = std::ranges::find_if(m_sources, [&file](auto &info){return info.fs->contains(file);});
            assert(it!=m_sources.end());
            if(m_files.contains(file)) m_files[file].source->unload(file);
            m_files[file].source = it->fs.get();
            return it->fs->load(file);
        }
        auto unload(const path &file) -> void override
        {
            m_files[file].source->unload(file);
        }
        auto has_changed(const path &file) -> bool override
        {
            auto it = std::ranges::find_if(m_sources, [&file](FSInfo &info){return info.fs->contains(file);});
            assert(it!=m_sources.end());

            if(m_files.contains(file) && m_files[file].source!=it->fs.get()) return true; // if source changes prop best to assume data changed

            if(auto *t = dynamic_cast<EditTracker*>(it->fs.get()); t!=nullptr)
                return t->has_changed(file);
            return false;
        }
        auto contains(const path &file) -> bool override
        {
            auto it = std::ranges::find_if(m_sources, [&file](auto &info){return info.fs->contains(file);});
            return it!=m_sources.end();
        }
    };

    class DependencyTracker : public FS
    {
        const path m_save;
        std::unordered_set<path> dependencies;
    public:
        DependencyTracker(path save = "")
            : m_save(std::move(save))
        {}
        auto get() const -> std::vector<path>;
        auto save() const -> void;
    };
}

