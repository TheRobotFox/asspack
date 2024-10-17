#ifndef ASSET_H_
#define ASSET_H_

#include <string>

namespace AssPack {

    template<typename T>
    class AssetManager;

    template<typename T>
    class AssetHandler
    {
        friend AssetManager<T>;
        AssetHandler() = delete;
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
            : ref(AssetManager<T>::get().load(name))
        {}
        auto has_changed() -> bool;
    };



}


#endif // ASSET_H_
