#ifndef ASSET_H_
#define ASSET_H_

#include "FS.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <pstl/glue_execution_defs.h>
#include <string>
#include <functional>
#include <list>
#include <cassert>
#include <memory>
#include <thread>
#include <unordered_map>

namespace asspack{

    template<typename T>
    class AssetHandle;

    template<typename T>
    class Asset;


    class AssetManager
    {
    public:
        struct Info {
            enum State {
                NONE,
                LOADED,
                BROKEN,
                NOFILE
        } state {NONE};
            uint32_t version {};
        };
    private:
        template<typename T>
        struct Assets {
            struct Derivitate {
                std::unique_ptr<std::function<T(const T&) const>> projection;
                std::shared_ptr<T> val {}; // TODO weak ptr root
                                         // unique Assets
                                         // vs global ?
                std::optional<T> update;
            };
            std::shared_ptr<T> root {};
            std::optional<T> update {};
            Info info {};
            std::list<Derivitate> derivitives {};
        };
        template<typename T>
        static auto load(const std::string &file) -> void
        {
            assert(m_fs!=nullptr);
            Assets<T> &asset = m_data<T>[file];

            if(!m_fs->contains(file)){
                asset.info.state = Info::State::NOFILE;
                return;
            }

            std::span<uint8_t> data = m_fs->load(file);
            std::optional<T> loaded = Asset<T>::load(data);
            if(!loaded){
                asset.info.state = Info::State::BROKEN;
                return;
            }
        }

        template<typename T>
        static auto getAssetGroup(const std::string &name) -> Assets<T>&
        {

            if(!m_data<T>.contains(name)){

                m_data<T>[name]={};

                if(m_fs)
                    load<T>(name);

                static bool updateFunction = false;
                if(!updateFunction){
                    updateFunction = true;
                    m_updateFunctions.push_back([&data=m_data<T>](){

                        std::for_each(std::execution::par_unseq,
                                    data.begin(),
                                    data.end(),
                                    [](std::pair<std::string, Assets<T>> &pair)
                            {

                                Assets<T> &assets = pair.second;
                                if(!assets.update) return;

                                *(assets.root) = *(assets.update);

                                std::list<typename Assets<T>::Derivitate> &d = assets.derivitives;
                                std::for_each(std::execution::par_unseq, d.begin(), d.end(),
                                    [&root=assets.root](typename Assets<T>::Derivitate &d)
                                    {
                                        if(!d.update) return;
                                        *(d.val) = *(d.update);
                                    });
                            });
                    });
                }
            }
            return  m_data<T>[name];
        }
    public:
        static auto Init() -> void;
        static auto Shutdown() -> void;

        static auto setFS(fs::FS *fs);
        static auto update() -> void
        {
            std::for_each(std::execution::par_unseq,
                          m_updateFunctions.begin(),
                          m_updateFunctions.end(),
                          [](auto &fn){fn();});
        }

        template<typename T>
        static auto createHandle(const std::string &name) -> AssetHandle<T>
        {
            Assets<T> &assets = getAssetGroup<T>(name);
            return {assets.root, assets.info};
        }
        template<typename T>
        static auto createDerivitaveHandle(const std::string &name, std::function<T(T)> &&map) -> AssetHandle<T>
        {
            Assets<T> &assets = getAssetGroup<T>(name);
            AssetHandle<T> handler {std::shared_ptr<T>(), assets.info};
            assets.derivitives.emplace_back(map, handler.m_val, std::nullopt);
            return {assets.root, assets.info};
        }

    private:

        std::thread tracker;
        bool run = true;
        template<typename T>
        static inline std::unordered_map<std::string, Assets<T>> m_data {};
        static inline std::vector<std::function<void()>> m_updateFunctions;
        static inline std::unique_ptr<fs::FS> m_fs {};

    };

    template<typename T>
    class AssetHandle
    {
        friend AssetManager;

        std::shared_ptr<T> m_val;

        const AssetManager::Info &m_info;
        int m_localVersion;
        AssetHandle(std::shared_ptr<T> val, const AssetManager::Info &info)
            : m_val(std::move(val)),
                m_info(info),
                m_localVersion(info.version)
        {}
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

    template<typename T>
    class Asset : public AssetHandle<T>
    {
        friend AssetManager;
        static auto load(const std::span<uint8_t>) -> std::optional<T> {
            static_assert(false, "Asset::load was not Implemented for used Type!\nPlease supply via template Specialisation");
        };
        static auto getDefault(AssetManager::Info::State state) -> T {
            static_assert(false, "Asset::load was not Implemented for used Type!\nPlease supply via template Specialisation");
        }
    public:
        Asset(const std::string &name) : AssetHandle<T>(AssetManager::createHandle<T>(name))
        {}
    };



}


#endif // ASSET_H_
