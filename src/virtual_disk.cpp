#include "nexa/virtual_disk.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace nexa {

VirtualDisk::VirtualDisk(std::string path, std::size_t capacity)
    : path_(std::move(path)), bytes_(capacity, 0) {
    if (capacity == 0) {
        throw std::invalid_argument("Disk size must be positive");
    }
    if (path_.empty()) {
        throw std::invalid_argument("Disk image path must not be empty");
    }
}

void VirtualDisk::load() {
    std::error_code error;
    const bool exists = std::filesystem::exists(path_, error);
    if (error) {
        throw std::runtime_error("Cannot inspect disk image: " + error.message());
    }
    if (!exists) {
        std::fill(bytes_.begin(), bytes_.end(), 0);
        return;
    }

    const auto fileSize = std::filesystem::file_size(path_, error);
    if (error) {
        throw std::runtime_error("Cannot read disk image size: " + error.message());
    }
    if (fileSize != bytes_.size()) {
        throw std::runtime_error("Disk image size does not match configured capacity");
    }

    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open disk image for reading");
    }
    file.read(reinterpret_cast<char*>(bytes_.data()),
              static_cast<std::streamsize>(bytes_.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(bytes_.size())) {
        throw std::runtime_error("Disk image is truncated or could not be read completely");
    }
}

void VirtualDisk::flush() const {
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Cannot open disk image for writing");
    }
    file.write(reinterpret_cast<const char*>(bytes_.data()),
               static_cast<std::streamsize>(bytes_.size()));
    file.flush();
    if (!file) {
        throw std::runtime_error("Disk image write failed");
    }
}

void VirtualDisk::write(std::size_t offset, const std::vector<std::uint8_t>& data) {
    if (offset > bytes_.size() || data.size() > bytes_.size() - offset) {
        throw std::out_of_range("Disk write out of range");
    }
    std::copy(data.begin(), data.end(),
              bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> VirtualDisk::read(std::size_t offset, std::size_t length) const {
    if (offset > bytes_.size() || length > bytes_.size() - offset) {
        throw std::out_of_range("Disk read out of range");
    }
    return {bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes_.begin() + static_cast<std::ptrdiff_t>(offset + length)};
}

} // namespace nexa
