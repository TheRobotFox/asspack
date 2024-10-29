#ifndef ASSET_H_
#define ASSET_H_

#include "FS.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <execution>
#include <string>
#include <functional>
#include <thread>
#include <tuple>
#include <unordered_map>

namespace asspack{

    template<typename T>
    class Asset;

    struct Info {
        enum State {
            NONE = 0,
            LOADED,
            BROKEN,
            NOFILE
        };
        State state {NONE};
        uint32_t version {};
    };

    class AssetGroupIface {
    public:
        virtual auto preload(const fs::path &file, const fs::FS *fs) -> void = 0;
        virtual auto update() -> void = 0;
        virtual auto destroyOrphans() -> int = 0;
        virtual ~AssetGroupIface() = default;
    };

    template<typename T>
    class AssetGroup : public AssetGroupIface
    {
    public:
        class Derivitive {
            std::function<T(const T&)> m_projection;
            std::shared_ptr<T> m_val;
            T m_prefetched;
            public:
                Derivitive(std::function<T(const T&)> projection)
                    : m_projection(projection),
                        m_val(new T)
                {}

            auto preload(const T &root)
            { m_prefetched = m_projection(root);}

            auto update()
            { *m_val = m_prefetched;}

            auto isDead() const -> bool
            {
                return m_val.use_count()<=1;
            }
            auto getHandle(Info &parrent) -> Asset<T>
            {
                return {m_val, parrent};
            }
        };
    private:
        struct Update {
            T val;
            Info::State state;
        };

        std::shared_ptr<T> m_root;
        std::optional<Update> m_update {};
        Info m_info {};
        std::list<Derivitive> m_derivitives {};
        fs::file_time m_lastChange {};

        std::mutex m_mtxUpd;
        auto preload_default(Info::State state) const -> Update
        {return {Asset<T>::getDefault(state), state};}

        auto fetch(const fs::path &file, const fs::FS *fs) const -> std::optional<Update>
        {
            if(!fs->contains(file)) return preload_default(Info::State::NOFILE);

            const auto *t = dynamic_cast<const fs::EditTracker*>(fs);
            if(t==nullptr || !t->has_changed(file, m_lastChange)) return {};

            auto data = fs->load(file);
            auto asset = Asset<T>::load(data);
            fs->unload(&data); // TODO allow Asset to own memory?

            if(!asset) return preload_default(Info::State::BROKEN);

            return Update{std::move(*asset), Info::State::LOADED};
        }

    public:
        AssetGroup()
            : m_root(new T)
        {}
        auto update() -> void override
        {
            if(!(m_update && m_mtxUpd.try_lock())) return;

            *m_root = std::move(m_update.value().val);
            m_info.state = m_update.value().state;
            m_info.version++;
            std::for_each(std::execution::par, m_derivitives.begin(), m_derivitives.end(), [](auto &d){d.update();});

            m_mtxUpd.unlock();
        }
        auto preload(const fs::path &file, const fs::FS *fs) -> void override
        {
            std::optional<Update> update;
            if(!(update = fetch(file, fs))) return;

            std::lock_guard<std::mutex> lock(m_mtxUpd);

            m_update = std::move(update);
            std::for_each(std::execution::par, m_derivitives.begin(), m_derivitives.end(),
                            [&newVal=m_update.value().val](Derivitive &d){d.preload(newVal);});
        }
        auto destroyOrphans() -> int override
        {
            int res = std::ranges::count_if(m_derivitives, &Derivitive::isDead);
            if(res>0) std::erase_if(m_derivitives, [](auto &d){return d.isDead();});
            return res;
        }
        auto createDerivitive(std::function<T(const T&)> projection) -> Asset<T>
        {
            return m_derivitives.emplace_back(projection).getHandle(m_info);
        }
        auto getHandle() -> Asset<T>
        {
            return {m_root, m_info};
        }
    };

    class AssetManager
    {


        template<typename T>
        auto getAssetGroup(const std::string &name) -> AssetGroup<T>*
        {
            if(m_data.contains(name)){
                auto *p = dynamic_cast<AssetGroup<T>*>(m_data[name].get());
                assert(p);
                return p;
            }
            m_data[name] = std::make_unique<AssetGroup<T>>();

            if(m_fs){
                m_data[name]->preload(name, m_fs.get());
                m_data[name]->update();
            }
            return dynamic_cast<AssetGroup<T>*>(m_data[name].get());
        }
        static auto foreachAsset(std::unordered_map<fs::path, std::unique_ptr<AssetGroupIface>> &assets,
                                    std::function<void(const fs::path&, std::unique_ptr<AssetGroupIface>&)> f)
        {
            std::for_each(std::execution::par, assets.begin(), assets.end(), [f](auto &pair){
                std::apply(f, pair);
            });
        }
    public:
        auto Tracker() -> void
        {
            m_tracker = std::thread([&run=run,
                                     &assets=m_data,
                                   &fs=m_fs,
                                   &mtxFS=m_mtxFs]{
                while(run){
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    std::lock_guard<std::mutex> fsLock(mtxFS);
                    if(!fs) continue;
                    foreachAsset(assets,
                                 [fs=fs.get()](auto &file, auto &asset)
                                 {asset->preload(file, fs);
                                     asset->destroyOrphans();});
                }
            });
            m_tracker.detach();
        }

        auto setFS(std::unique_ptr<fs::FS> fs) -> void
        {
            m_mtxFs.lock();
            m_fs = std::move(fs);

            foreachAsset(m_data, [fs=m_fs.get()](auto &file, auto &asset){
                asset->preload(file, fs);
            });

            m_mtxFs.unlock();
            update();
        }
        auto update() -> void
        {
            foreachAsset(m_data, [fs=m_fs.get()](auto & /* ignore */, auto &asset)
                {
                    asset->update();
                });
        }

        template<typename T>
        auto get(const std::string &name) -> Asset<T>
        {
            return getAssetGroup<T>(name)->getHandle();
        }
        template<typename T>
        auto get(const std::string &name, std::function<T(T)> &&projection) -> Asset<T>
        {
            return getAssetGroup<T>(name)->createDerivitive(projection);
        }

    private:

        std::thread m_tracker;
        bool run = true;
        std::unordered_map<fs::path, std::unique_ptr<AssetGroupIface>> m_data;
        std::unique_ptr<fs::FS> m_fs;
        std::mutex m_mtxFs;
    };

    template<typename T>
    class Asset
    {
        friend AssetGroup<T>;
        friend typename AssetGroup<T>::Derivitive;

        std::shared_ptr<T> m_val;

        const Info &m_info;
        int m_localVersion;
        Asset(std::shared_ptr<T> val, const Info &info)
            : m_val(std::move(val)),
                m_info(info),
                m_localVersion(info.version)
        {}
        static auto load(const std::span<const uint8_t> /* ignore */) -> std::optional<T> {
            static_assert(false, "Asset::load was not Implemented for used Type!\nPlease supply via template Specialisation");
        };
        static auto getDefault(Info::State /* ignore */) -> T& {
            static T obj{};
            return obj;
        }
    public:
        auto get() -> const T& {
            if(m_info.state==Info::State::LOADED)
                return *m_val;
            return Asset<T>::getDefault(m_info.state);
        }

        operator const T&() {return get();}
        auto has_changed() const -> bool {return m_localVersion == m_info.version;}
        auto fetch_version()     -> bool
        {
            bool res = this->has_changed();
            m_localVersion = m_info.version;
            return res;
        }

    };

}


#endif // ASSET_H_
