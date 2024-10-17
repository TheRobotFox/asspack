#include "Asset.hpp"
#include <thread>
#include <unordered_map>

namespace AssPack{

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

}
