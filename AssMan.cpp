#include "Asset.hpp"
#include <utility>

namespace AssPack {

    template<typename T>
    auto AssetManager::load(const std::string &name) -> Asset<T>
    {
        // register assets, create Handles
        // Assets will be loaded once a FS has been registered
        if(!m_data<T>.contains(name))
            m_data<T>[name] = {};

        if()
        m_data<T>[name]
    }

}
