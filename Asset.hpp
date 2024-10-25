#ifndef ASSET_H_
#define ASSET_H_

#include "FS.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <pstl/glue_execution_defs.h>
#include <string>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <unordered_map>

namespace asspack{

    template<typename T>
    class Asset;

    struct Info {
        enum State {
            NONE,
            LOADED,
            BROKEN,
            NOFILE
        };
        State state {NONE};
        uint32_t version {};
    };

    class AssetManager
    {
    private:
            // static inline std::size_t _counter = 0;
            // template<typename T>
            // static inline std::size_t typeIdx = _counter++; // for Asset<T> Groups (maybe faster)

        class AssetGroupIface {
            virtual auto preload(std::span<uint8_t> data) -> void = 0;
            virtual auto update() -> void = 0;
        };

        template<typename T>
        class AssetGroup : public AssetGroupIface
        {
            struct Derivitate {
                std::function<T(const T&)> projection;
                std::shared_ptr<T> val {};
                std::optional<T> update;
            };
            struct Update {
                T val;
                Info::State state;
            };

            std::shared_ptr<T> m_root {};
            std::optional<Update> m_update {};
            Info m_info {};
            std::list<Derivitate> m_derivitives {};

            std::mutex m_mtxUpd;
            std::mutex m_mtxSet;
        public:
            auto update() -> void;
            auto preload(std::span<uint8_t> data) -> void override;
            auto createHandle(std::function<T(const T&)> projection) -> Asset<T>;
            auto getHandle() -> Asset<T>;
        };

        auto preload(const fs::path &file, AssetGroupIface *asset) -> void
        {
            if(!m_fs->contains(file)) return;
            auto *t = dynamic_cast<fs::EditTracker*>(m_fs.get());
            if(t==nullptr || t->has_changed(const path &file, file_time from))
        }
        template<typename T>
        auto getAssetGroup(const std::string &name) -> AssetGroup<T>*
        {
            if(m_data.contains(name)){
                auto *p = dynamic_cast<AssetGroup<T>*>(m_data[name].get());
                assert(p);
                return p;
            }
            m_data[name] = std::make_unique<AssetGroup<T>>(new AssetGroup<T>(name));

            if(m_fs){
                m_data[name]->preload();
                update(m_data<T>[name]);
            }

        }
    public:
        static auto Tracker() -> void
        {
            tracker = std::thread([&run=run,
                                   &reloadFn=m_reloadFunctions,
                                   &fs=m_fs,
                                   &mtxFS=m_mtxFs]{
                while(run){
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    std::lock_guard<std::mutex> fsLock(mtxFS);
                    if(!fs) continue;

                    std::for_each(std::execution::par,
                                  reloadFn.begin(),
                                  reloadFn.end(),
                                  [](auto fn){fn();});
                }
            });
            tracker.detach();
        }

        static auto setFS(std::unique_ptr<fs::FS> fs) -> void
        {
            m_mtxFs.lock();
            m_fs = std::move(fs);

            // std::ranges::for_each(m_reloadFunctions, &std::function<void()>::operator());

            m_mtxFs.unlock();
            update();
        }
        static auto update() -> void
        {
            std::for_each(std::execution::par,
                          m_updateFunctions.begin(),
                          m_updateFunctions.end(),
                          [](auto &fn){fn();});
        }

        template<typename T>
        auto get(const std::string &name) -> AssetHandle<T>
        {
        }
        template<typename T>
        auto get(const std::string &name, std::function<T(T)> &&map) -> AssetHandle<T>
        {
        }

    private:

        std::thread tracker;
        bool run = true;
        std::unordered_map<std::string, std::unique_ptr<AssetGroupIface>> m_data {};
        std::unique_ptr<fs::FS> m_fs {};
    };

    template<typename T>
    class Asset
    {
        friend AssetManager;

        std::shared_ptr<T> m_val;

        const AssetManager::Info &m_info;
        int m_localVersion;
        Asset(std::shared_ptr<T> val, const AssetManager::Info &info)
            : m_val(std::move(val)),
                m_info(info),
                m_localVersion(info.version)
        {}
        static auto load(const std::span<uint8_t> /* ignore */) -> std::optional<T> {
            static_assert(false, "Asset::load was not Implemented for used Type!\nPlease supply via template Specialisation");
        };
        static auto getDefault(AssetManager::Info::State state) -> T& {
            static T obj{};
            return obj;
        }
    public:
        auto get() -> const T& {
            if(m_info.state==AssetManager::Info::State::LOADED)
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
