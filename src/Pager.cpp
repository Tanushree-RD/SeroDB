#include "serodb/Pager.hpp"

#include "serodb/Constants.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string>

namespace serodb {

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

Pager::Pager(const std::string& filename)
    : filename_(filename)
{
    // Open the file for both reading and writing in binary mode.
    // std::fstream will *not* create the file when opened with in|out, so
    // we first try the normal open and, if it fails (file doesn't exist),
    // create the file with a truncating open and then re‑open for read/write.
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file_.is_open()) {
        // File does not exist yet — create it.
        std::ofstream create(filename_, std::ios::binary | std::ios::trunc);
        if (!create) {
            throw std::runtime_error("Pager: could not create database file: " + filename_);
        }
        create.close();

        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            throw std::runtime_error("Pager: could not open database file after creation: " + filename_);
        }
    }

    // Determine the current file size.
    file_.seekg(0, std::ios::end);
    file_length_ = static_cast<std::size_t>(file_.tellg());
}

Pager::~Pager()
{
    // Best‑effort flush on destruction — errors are silently swallowed
    // because destructors must not throw.
    try {
        close();
    } catch (...) {}
}

// -----------------------------------------------------------------------
// Page access
// -----------------------------------------------------------------------

char* Pager::get_page(std::size_t page_num)
{
    if (page_num >= MAX_PAGES) {
        throw std::runtime_error("Pager: page number out of bounds ("
                                 + std::to_string(page_num) + " >= "
                                 + std::to_string(MAX_PAGES) + ")");
    }

    if (!pages_[page_num]) {
        // Allocate a fresh page buffer, zero‑filled.
        pages_[page_num] = std::make_unique<PageBuffer>();
        std::memset(pages_[page_num]->data(), 0, PAGE_SIZE);

        // If the file already contains data for this page, read it in.
        const std::size_t offset = page_num * PAGE_SIZE;

        if (offset < file_length_) {
            // How many bytes of this page actually exist on disk?
            const std::size_t bytes_on_disk =
                std::min(PAGE_SIZE, file_length_ - offset);

            file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            file_.read(pages_[page_num]->data(),
                       static_cast<std::streamsize>(bytes_on_disk));

            if (!file_) {
                // Clear the error state so the file remains usable.
                file_.clear();
                throw std::runtime_error("Pager: failed to read page "
                                         + std::to_string(page_num));
            }
        }
        // Pages beyond file_length_ are already zeroed — they represent
        // newly allocated pages that will be written on the next flush.
    }

    return pages_[page_num]->data();
}

void Pager::flush(std::size_t page_num)
{
    if (page_num >= MAX_PAGES) {
        throw std::runtime_error("Pager: flush page number out of bounds");
    }

    if (!pages_[page_num]) {
        // Nothing to flush — the page was never loaded or allocated.
        return;
    }

    const std::size_t offset = page_num * PAGE_SIZE;

    file_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    file_.write(pages_[page_num]->data(),
                static_cast<std::streamsize>(PAGE_SIZE));

    if (!file_) {
        throw std::runtime_error("Pager: failed to flush page "
                                 + std::to_string(page_num));
    }

    file_.flush();
}

void Pager::close()
{
    if (!file_.is_open()) {
        return;
    }

    // Flush every loaded page.
    for (std::size_t i = 0; i < MAX_PAGES; ++i) {
        if (pages_[i]) {
            flush(i);
            pages_[i].reset(); // release memory
        }
    }

    file_.close();
}

// -----------------------------------------------------------------------
// Stats
// -----------------------------------------------------------------------

std::size_t Pager::pages_loaded() const
{
    std::size_t count = 0;
    for (const auto& p : pages_) {
        if (p) { ++count; }
    }
    return count;
}

} // namespace serodb
