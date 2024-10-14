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

    // StorageTracker
    auto Storage::has_changed(path file) -> bool
    {
        assert(m_files.contains(file));

        file_time current = std::filesystem::last_write_time(file);
        return current!=m_files[file].last_change;
    }

    auto Storage::get() const -> std::vector<path> {
        std::vector<path> out;
        std::ranges::transform(m_files, out, [](auto &pair) { return pair.first; });
        return out;
    }

} // namespace asspack::fs
