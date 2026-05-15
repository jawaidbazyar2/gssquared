#pragma once

#include <string>
#include <vector>
#include <fstream>

#include "paths.hpp"

class PMap {
    private:
        std::string pmap_filename; 
        std::vector<std::string> filenames;

        public:
        PMap(const std::string& filename) : pmap_filename(filename) {
            read_pmap_file();
        }
        ~PMap() {
            filenames.clear();
        }

        std::vector<std::string> get_filenames() {
            return filenames;
        }

        void read_pmap_file() {
            // Get the directory of the pmap file
            std::string directory = Paths::get_directory(pmap_filename);

            std::ifstream file(pmap_filename);
            std::string line;
            while (std::getline(file, line)) {
                // if filename is a relative path, make it absolute, by prepending the directory
                if (!Paths::is_absolute(line)) {
                    line = directory + "/" + line;
                }
                filenames.push_back(line);
            }
            file.close();
        }
};
