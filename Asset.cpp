#include "Asset.hpp"
namespace asspack {

        auto update() -> void
        {
            if(!m_update) return;
            std::cout << "Updated: " << std::endl;

            std::lock_guard<std::mutex> lock(m_mtxSet);

            *(m_root) = std::move(m_update.value().val);
            m_info.state = m_update.value().state;
            m_update = {};

            std::for_each(std::execution::par, m_derivitives.begin(), m_derivitives.end(),
                [](Derivitate &d)
                {
                    if(!d.update) return;
                    *(d.val) = std::move(*(d.update));
                    d.update = {};
                });
        }
        auto fetch(fs::FS *fs) -> void override
        {
            Info::State state;

            if(!fs->contains(m_file)){
                state = Info::State::NOFILE;
                return;
            }
            if(!dynamic_cast<fs::EditTracker*>(fs)->has_changed(m_file)) return;
            std::cout << "Changed: " << m_file << std::endl;

            std::span<uint8_t> data = fs->load(m_file);
            std::optional<T> loaded = Asset<T>::load(data);
            if(!loaded){
                state = Info::State::BROKEN;
                return;
            }
            state = Info::State::LOADED;


            std::lock_guard<std::mutex> lock(m_mtxUpd);

            m_update.emplace(*loaded, state);
            std::for_each(std::execution::par, m_derivitives.begin(), m_derivitives.end(),
                [&loaded](Derivitate &d)
                {
                    std::cout << "update D" << std::endl;
                    (d.update) = d.projection(*loaded);
                });
        }
        auto createHandle(std::function<T(const T&)> projection) -> Asset<T>
        {
            Derivitate &d = m_derivitives.emplace_back(projection, std::shared_ptr<T>(new T));
            return {d.val, d.projection};
        }
        auto getHandle() -> Asset<T>
        {
            return {m_root, m_info};
        }


    private:
    };
};
