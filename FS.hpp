#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
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

    class FS
    {
    public:
        virtual auto load(const path &) const -> std::span<const uint8_t> = 0;
        virtual auto contains(const path &) const -> bool = 0;
            virtual auto unload(std::span<const uint8_t> *data) const -> void = 0;
        virtual ~FS() = default;
    };

    class EditTracker
    {
    public:
            virtual auto has_changed(const path &file, file_time from) const -> bool = 0; // TODO Thread safe
            virtual auto getTime(const path &file) const -> file_time = 0;
    };

    class Storage : public FS, public EditTracker
    {
        const path m_directory;

    public:
        Storage(path directory="") : m_directory(std::move(directory))
        {}

        auto load(const path &file) const -> std::span<const uint8_t> override
        {
            return read(m_directory/file);
        }
        auto unload(std::span<const uint8_t> *data) const -> void override
        {
            delete [] data->data();
            *data = {};
        }
        auto has_changed(const path &file, file_time from) const -> bool override
        {
            return std::filesystem::last_write_time(file)!=from;
        }
        auto getTime(const path &file) const -> file_time override
        {
            return std::filesystem::last_write_time(file);
        }
        auto contains(const path &file) const -> bool override
        {
            return std::filesystem::exists(file);
        }

        static auto read(const path &path) -> std::span<uint8_t>
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            file.unsetf(std::ios::skipws);

            std::size_t length = static_cast<std::size_t>(file.tellg());
            auto *data = new uint8_t[length];
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(data), length);
            return {data, length};
        }
    };

    /*class Memory : public FS
    {
        struct Block {
            std::size_t offset;
            std::size_t size;
        };

    protected:
        using Header = std::unordered_map<path, Block>;
        const std::optional<std::span<const uint8_t>> m_own {};
        Header m_header;
        std::span<const uint8_t> m_data;

        template<class T>
        static auto read(const uint8_t *&ptr) -> T
        {
            T res = *reinterpret_cast<const T*>(ptr);
            ptr+=sizeof(T);
            return res;
        }
        static auto read_name(const uint8_t *&ptr) -> std::string_view
        {
            auto len = read<uint16_t>(ptr);
            std::string_view name(reinterpret_cast<const char*>(ptr), len);
            ptr+=len;
            return name;
        }
        auto from_span(std::span<const uint8_t> data) -> void
        {
            const uint8_t *ptr = data.data();
            auto entries = read<uint16_t>(ptr);

            while(entries-- > 0) {
                auto name = read_name(ptr);
                auto offset = read<std::size_t>(ptr);
                auto size = read<std::size_t>(ptr);

                m_header[name] = {offset, size};
            }
            m_data = {ptr, m_data.end().base()};
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

        auto load(const path &file) const -> std::span<const uint8_t> override
        {
            const Block &b = m_header.at(file);
            return m_data.subspan(b.offset, b.size);
        }
        auto unload(std::span<const uint8_t> *data) const -> void override
        {
            *data = {};
        }
        auto contains(const path &file) const -> bool override
        {
            return m_header.contains(file);
        }
        };*/

    /*class AssetFiles : public FS, public EditTracker
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
        auto getFileFS(const path &file)
        {
            auto it = std::ranges::find_if(m_sources, [&file](auto &info){return info.fs->contains(file);});
            assert(it!=m_sources.end());
            return it;
        }
    public:
        template<class F>
            requires std::is_base_of_v<FS, F>
        auto add_top(std::string name, std::unique_ptr<F> &&fs) -> void
        {
            m_sources.emplace_back(std::move(name), std::move(fs));
        }
        template<class F>
            requires std::is_base_of_v<FS, F>
        auto add_bottom(std::string name, std::unique_ptr<F> &&fs) -> void
        {
            m_sources.emplace_front(std::move(name), std::move(fs));
        }
        auto remove(const std::string &name) -> void
        {
            auto it = std::ranges::find_if(m_sources, [&name](FSInfo &info){return info.name==name;});
            if(it==m_sources.end()) return;

            // free all files associatet with FS(name)
            std::erase_if(m_files, [&it](const std::pair<path, FileInfo> &info){return info.second.source==it->fs.get();});
            m_sources.erase(it);
        }

        auto load(const path &file) const -> std::span<uint8_t> override
        {
            auto it = getFileFS(file);
            if(m_files.contains(file)) m_files[file].source->unload(file);
            m_files[file].source = it->fs.get();
            return it->fs->load(file);
        }
        auto unload(const path &file) -> void override
        {
            m_files[file].source->unload(file);
        }
        auto has_changed(const path &file, file_time from) -> bool override
        {
            auto it = getFileFS(file);

            if(m_files.contains(file) && m_files[file].source!=it->fs.get()) return true; // if source changes prop best to assume data changed

            if(auto *t = dynamic_cast<EditTracker*>(it->fs.get()); t!=nullptr)
                return t->has_changed(file, from);
            return false;
        }
        auto getTime(const path &file) -> file_time override
        {
            auto it = getFileFS(file);
            if(auto *t = dynamic_cast<EditTracker*>(it->fs.get())) return t->getTime(file);
            return {};
        }
        auto contains(const path &file) -> bool override
        {
            auto it = std::ranges::find_if(m_sources, [&file](auto &info){return info.fs->contains(file);});
            return it!=m_sources.end();
        }
    };*/

    class DependencyTracker : public FS
    {
        const path m_save;
        std::unordered_set<path> dependencies; // TODO Thread safety
    public:
        DependencyTracker(path save = "")
            : m_save(std::move(save))
        {}
        auto get() const -> std::vector<path>;
        auto save() const -> void;
        ~DependencyTracker() override;
    };
}

