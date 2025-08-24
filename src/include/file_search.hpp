#ifndef FILE_SEARCH_HPP
#define FILE_SEARCH_HPP

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>

// Include protobuf headers
#include "file_deatils.pb.h"

class FileSearch {
private:
    std::string metadata_path_;
    std::map<std::string, file_details::FileDetails> file_index_;

public:
    FileSearch(const std::string& metadata_path);
    
    void load_file_index();
    void save_file_index();
    bool add_file(const file_details::FileDetails& file_details);
    bool remove_file(const std::string& file_name);
    std::optional<file_details::FileDetails> find_file(const std::string& file_name);
    std::vector<file_details::FileDetails> search_files(const std::string& pattern);
    std::vector<file_details::FileDetails> list_files();
    bool file_exists(const std::string& file_name);
    size_t get_file_count() const;
    void print_file_index() const;
};

#endif // FILE_SEARCH_HPP 