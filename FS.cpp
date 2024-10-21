#include "FS.hpp"
#include <cassert>
#include <fstream>
#include <iterator>


namespace asspack::fs {


    auto Storage::read(const path &path) -> std::vector<uint8_t>
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        file.unsetf(std::ios::skipws);

        std::vector<uint8_t> res;
        res.reserve(static_cast<std::size_t>(file.tellg()));
        file.seekg(0, std::ios::beg);
        std::copy(std::istream_iterator<uint8_t>(file),
                std::istream_iterator<uint8_t>(),
                std::back_inserter(res));
        return res;
    }
}
