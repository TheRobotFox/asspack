#include "FS.hpp"
#include <cassert>
#include <fstream>
#include <iterator>


namespace asspack::fs {


    auto Storage::read(path path) -> std::vector<uint8_t>
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

    auto Memory::read(path file) -> std::vector<uint8_t>
    {
        assert(m_header.contains(file));
        info info = m_header[file];
        // std::cout << name << " " << info.offset << " " << info.size << std::endl;

        uint8_t *start = m_data.get()+info.offset;
        std::vector<uint8_t> v;
        v.assign(start, start+info.size);
        return v;
    }
}
