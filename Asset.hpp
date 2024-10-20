#ifndef ASSET_H_
#define ASSET_H_

#include "FS.hpp"
#include <algorithm>
#include <bits/ranges_algo.h>
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
#include <unordered_set>

namespace asspack{

    template<typename T>
    class Asset;
    class AssetManager
    {
    public:
        struct Info {
            enum {
                UNINIT,
                LOADED,
                BROKEN
        } state {UNINIT};
            uint32_t version {};
        };
    private:
        template<typename T>
        struct Assets {
            struct Derivitate {
                std::function<T(const T&) const> projection;
                std::weak_ptr<T> val {};
                std::optional<T> update;
            };
            std::shared_ptr<T> root {};
            std::optional<T> update {};
            Info info {};
            std::list<Derivitate> derivitives {};
        };
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
        static auto load(const std::string &name) -> Asset<T>
        {

            if(!m_data<T>.contains(name)) m_data<T>[name]={};

            if()
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

                            assets.root = *assets.update;

                            std::list<typename Assets<T>::Derivitate> &d = assets.derivitives;
                            std::for_each(std::execution::par_unseq, d.begin(), d.end(),
                                [&root=assets.root](typename Assets<T>::Derivitate &d)
                                {
                                    if(!d.update) return;
                                    *d.val = *d.update;
                                });
                        });
                });
            }
        }
    private:

        std::thread tracker;
        bool run = true;
        template<typename T>
        static inline std::unordered_map<std::string, Assets<T>> m_data {};
        static inline std::vector<std::function<void()>> m_updateFunctions;
        static inline FS

    };

    template<typename T>
    class Asset
    {
        std::shared_ptr<T> m_val;

        const AssetManager::Info &m_info;
        int m_localVersion;
    public:
        auto get() -> const T& {
            if(m_info.loaded)
                return *m_val;
            assert(false && "Defaults not implemented yet!");
        }

        operator const T&() {return get();}
        Asset(std::shared_ptr<T> &&val, const AssetManager::Info &info)
            : m_val(std::move(val)),
                m_info(info),
                m_localVersion(info.version)
        {}
        auto has_changed()   -> bool {return m_localVersion == m_info.version;}
        auto fetch_version() -> bool
        {
            bool res = this->has_changed();
            m_localVersion = m_info.version;
            return res;
        }
    };



}


#endif // ASSET_H_
