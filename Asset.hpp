#ifndef ASSET_H_
#define ASSET_H_

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

    template<typename T>
    class AssetHandler;

    template<typename T>
    class AssetManager
    {
        inline static AssetManager<T> *self {};
        std::thread tracker;
        bool run = true;
        std::unordered_map<std::string, AssetHandler<T>> data {};

    public:
        static void Init();
        static void Shutdown();

        auto get() -> AssetManager<T>&;
        auto load(const std::string &name) -> const AssetHandler<T>&;
    };


    template<typename T>
    class Asset
    {
        const AssetHandler<T> &ref;
        int revision;
    public:
        auto get() -> const T&;
        operator const T&();
        Asset(const std::string &name)
            : ref(AssetManager<T>::get().load(name));
        auto has_changed() -> bool;
        auto update_count() -> void;
    };

}


#endif // ASSET_H_
