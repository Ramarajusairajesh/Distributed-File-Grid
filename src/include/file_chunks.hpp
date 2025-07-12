#include <fstream>
#include <iostream>
#include <openssl/md5.h>
#include <regex>
#include <string>

#define CHUNK_FILE_SIZE 67108864

struct chunk
{
        int chunk_id;
        uint8_t rack_id;
};

struct chunk_storage
{
        std::string file_name;
};

struct metdata
{
        time_t timestamp;
        std::string MD5_hash;
};

int chunk_creations(const std::string &file_path)
{
        std::size_t chunknumber = 1;
        std::ifstream in(file_path, std::ios::binary);
        if (!in)
        {
                std::cerr << "Error while reading the file" << std::endl;
                return 1;
        }
        char *buffer = new char[CHUNK_FILE_SIZE];
        while (in)
        {
                in.read(buffer, CHUNK_FILE_SIZE);
                // return the number of character read
                std::streamsize bytesRead = in.gcount();
                if (bytesRead == 0)
                        break;

                std::string outFile = file_path + ".part" + std::to_string(chunknumber);
                std::ofstream out(outFile, std::ios::binary);
                if (!out)
                {
                        std::cerr << "Error while creating chunks" << std::endl;
                        delete[] buffer;
                        return 1;
                }
                out.write(buffer, bytesRead);
                ++chunknumber;
                out.close();
        }
        delete[] buffer;
        in.close();
        return 0;
}

int chunk_combine(char **chunk_files, int parts)
{
        // Extract original filename from the first chunk
        std::string chunk_name = chunk_files[1];
        std::regex part_regex(R"(\.part\d+$)");
        std::string original_filename = std::regex_replace(chunk_name, part_regex, "");

        // Allocate 64MB buffer
        char *buffer = new char[CHUNK_FILE_SIZE];

        // Open output file in binary mode
        std::ofstream out(original_filename, std::ios::binary);
        if (!out)
        {
                std::cerr << "Error opening output file: " << original_filename << std::endl;
                delete[] buffer;
                return 1;
        }

        // Process each chunk file
        for (int i = 1; i < parts; i++)
        {
                std::ifstream in(chunk_files[i], std::ios::binary);
                if (!in)
                {
                        std::cerr << "Error opening chunk file: " << chunk_files[i] << std::endl;
                        delete[] buffer;
                        out.close();
                        return 1;
                }

                // Read up to 64MB
                in.read(buffer, CHUNK_FILE_SIZE);
                std::streamsize bytesRead = in.gcount();

                // Write the data to output file
                if (bytesRead > 0)
                        out.write(buffer, bytesRead);

                in.close();
        }

        delete[] buffer;
        out.close();
        return 0;
}
