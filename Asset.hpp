#ifndef ASSET_H_
#define ASSET_H_

#include "Reader.hpp"
#include <cassert>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <ranges>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace AssPack {

    // Asset -> request file -> store id -> auto updates on change
    template<typename T>
    class AssetManager;

    template<typename T>
    class AssetHandler
    {
        friend AssetManager<T>;
        T asset;
        int revision {};

        auto load(const std::vector<uint8_t> &data) -> bool // maybe for mutex impls
        {
            revision++;
            bool res = load_impl(data);
            return res;
        };
        auto load_impl(const std::vector<uint8_t> &data) -> bool;

    public:

        operator const T&() const
        {
            return asset;
        }
        operator T&()
        {
            return asset;
        }
        int get_mod_count() const
        {
            return revision;
        }
    };

    template<typename T>
    class AssetManager
    {

        inline static AssetManager<T> *self {};

        std::thread tracker;
        bool run = true;

        std::unordered_map<std::string, AssetHandler<T>> data {};

        auto tracker_run() -> void
        {
            using namespace std::ranges;
            using namespace std::chrono_literals;
            auto &fs = asset_files;
            while(run) {
                std::this_thread::sleep_for(200ms);

                for_each(data | views::filter([](auto &pair){return fs.has_changed(pair.first);})
                         | views::filter([](auto &pair){
                             auto& [name, handle] = pair;
                             return !handle.load(fs.read(name));
                         }), [](auto &pair){std::cout << "[WARNING] "<< pair.first << " Failed to load!";});
                         
            }
        }

        AssetManager()
        {
            #ifndef MAPPED
            tracker = std::thread( [this]{tracker_run();} );
            tracker.detach();
            #endif
        }
        ~AssetManager()
        {
            run = false;
            tracker.join();
        }

    public:

        static auto get() -> AssetManager<T>&
        {
            if(self==nullptr){
                self = new AssetManager<T>;
                // spawn tracker
            }
            return *self;
        }

        auto load(const std::string &name) -> const AssetHandler<T>&
        {
            if(data.contains(name)) return data[name];

            data[name] = {};
            data[name].load(asset_files.read(name));
            return data[name];
        }

    };


    template<>
    auto AssetHandler<std::string>::load_impl(const std::vector<uint8_t> &data) -> bool
    {
        std::cout << "Loading!" << std::endl;
        asset = {data.begin(), data.end()};
        return true;
    }

    template<typename T>
    class Asset
    {
        const AssetHandler<T> &ref;
        int revision;
    public:
        auto get() -> const T&
        {
            return ref;
        }
        operator const T&()
        {
            return get();
        }
        Asset(const std::string &name)
            : ref(AssetManager<T>::get().load(name))
        {
            update_count();
        }
        auto has_changed() -> bool
        {
            return ref.get_mod_count()!=revision;
        }
        auto update_count() -> void
        {
            revision = ref.get_mod_count();
        }
    };

}


#endif // ASSET_H_
