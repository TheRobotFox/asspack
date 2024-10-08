#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <print>
#include <ranges>
#include <vector>
#include <string>
#include <algorithm>

auto read_dependencies(const std::string &deps_file) -> std::vector<std::string>
{
    using namespace std::ranges;
    std::ifstream file(deps_file);
    if(!file.good()){
        std::cout << "[ERROR] Failed to read dependency file!" << std::endl;
        exit(3);
    }
    std::vector<std::string> dependencies;
    dependencies.reserve(50);
    copy(istream_view<std::string>(file), std::back_inserter(dependencies));
    for( auto name: dependencies) std::cout << "[INFO] Packing " << name << std::endl;
    return dependencies;
}

auto write_header(std::ostream &out, std::span<std::string> dependencies, std::span<std::size_t> sizes) -> void
{
    using namespace std::ranges;
    uint16_t entries = dependencies.size();
    out.write(reinterpret_cast<char *>(&entries), sizeof(entries));

    fold_left(views::zip(dependencies, sizes), 0,
              [&out](size_t offset, std::pair<std::string_view, std::size_t> pair){
        auto& [name, size] = pair;
        uint16_t name_length = name.length();
        out.write(reinterpret_cast<char *>(&name_length), sizeof(name_length));
        out << name;
        out.write(reinterpret_cast<char *>(&offset), sizeof(offset));
        out.write(reinterpret_cast<char *>(&size), sizeof(size));
        return offset+size;
    });
}

auto write_body(std::ostream &out, std::span<std::ifstream> file_handles) -> void
{
    using namespace std::ranges;
    for_each(file_handles, [&out](std::ifstream &file){out << file.rdbuf();});
}

auto open_files(const std::string &asset_dir, std::span<std::string> names, std::vector<std::ifstream> &file_handles) -> bool
{
    using namespace std::ranges;
    file_handles.resize(names.size());
    return all_of(views::zip(names, file_handles) | views::transform(
        [&asset_dir](std::pair<const std::string&, std::ifstream&> pair) {
            auto [name, handle] = pair;
            handle.open(asset_dir + "/" + name); // open at end to get size
            if(!handle.good()){
                std::cout << "[ERROR] Could not find Resource \"assets/" << name << "\"" << std::endl;
                return false;
            }
            return true;
        }), std::identity());
}

auto file_sizes(std::span<std::ifstream> file_handles) -> std::vector<std::size_t>
{
    using namespace std::ranges;
    std::vector<std::size_t> sizes;
    copy(file_handles | views::transform([](auto &file){
        file.seekg(0, std::ios::end);
        std::size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        return size;
    }), std::back_inserter(sizes));
    return sizes;
}

int main(int argc, char **argv)
{

    if(argc!=4){
        std::println("Usage: {} [dependencies_in] [asset_dir] [packedResources_out]", argv[0]);
        return 1;
    }

    std::vector<std::string> dependencies = read_dependencies(argv[1]);
    std::vector<std::ifstream> file_handles;
    if(!open_files(argv[2], dependencies, file_handles)) return 2;
    std::vector<std::size_t> sizes = file_sizes(file_handles);

    std::ofstream out(argv[3], std::ios::binary);
    write_header(out, dependencies, sizes);
    write_body(out, file_handles);
}
