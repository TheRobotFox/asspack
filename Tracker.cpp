#include "FS.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <algorithm>

namespace asspack::fs {
    // Dependency Tracker Impl

    auto DependencyTracker::save() const -> void
    {
        std::cout << "Exporting Resourcepack!" << std::endl;

        std::ofstream out(m_save);
        for(const auto& path : get())
            out << path << std::endl;
    }

    DependencyTracker::~DependencyTracker()
    {
        if(m_save.empty()) return;
        save();
    }

} // namespace asspack::fs
