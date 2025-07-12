#include "file_chunks.hpp"

int main(int argc, char **argv)
{
        if (argc < 2)
        {
                std::cerr
                    << "Insufficient arguments for either chunks creationg or combining the chunks"
                    << std::endl;
                return 1;
        }

        else if (argc == 2)
        {
                chunk_creations(argv[1]);
        }
        else
        {
                chunk_combine(argv, argc);
        }
}
