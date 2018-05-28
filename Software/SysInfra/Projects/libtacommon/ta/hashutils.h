#pragma once

#include <string>
#include <vector>

namespace ta
{
    namespace HashUtils
    {
        /**
          Create hash of data/file in binary/hex format
          @note for calculating hashes of large files the "File"-family functions are preferred due to less footprint compared to "data" counterparts
         */

        std::vector<unsigned char> getMd5Bin(const std::string& aVal);
        std::vector<unsigned char> getMd5Bin(const std::vector<unsigned char>& aVal);
        std::vector<unsigned char> getMd5BinFile(const std::string& aFilePath);
        std::string getMd5Hex(const std::string& aVal);
        std::string getMd5Hex(const std::vector<unsigned char>& aVal);
        std::string getMd5HexFile(const std::string& aFilePath);

        std::vector<unsigned char> getSha1Bin(const std::string& aVal);
        std::vector<unsigned char> getSha1Bin(const std::vector<unsigned char>& aVal);
        std::vector<unsigned char> getSha1BinFile(const std::string& aFilePath);
        std::string getSha1Hex(const std::string& aVal);
        std::string getSha1Hex(const std::vector<unsigned char>& aVal);
        std::string getSha1HexFile(const std::string& aFilePath);

        std::vector<unsigned char> getSha256Bin(const std::string& aVal);
        std::vector<unsigned char> getSha256Bin(const std::vector<unsigned char>& aVal);
        std::vector<unsigned char> getSha256BinFile(const std::string& aFilePath);
        std::string getSha256Hex(const std::string& aVal);
        std::string getSha256Hex(const std::vector<unsigned char>& aVal);
        std::string getSha256HexFile(const std::string& aFilePath);
    }
}
