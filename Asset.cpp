#include "Asset.hpp"

namespace asspack {



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


}
