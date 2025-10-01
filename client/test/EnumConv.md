```c++
/**
 * @brief 测试枚举类型转换功能
 */
void TestEnumConversion() {
    std::cout << "\n🔄 [模块5] 枚举转换测试:" << std::endl;

    // 测试CompressionAlgorithm转换
    std::cout << "  🗜️  CompressionAlgorithm 转换测试:" << std::endl;
    std::vector<CompressionAlgorithm> compressionAlgos = {
        CompressionAlgorithm::NONE, CompressionAlgorithm::GZIP,
        CompressionAlgorithm::ZSTD, CompressionAlgorithm::LZ4,
        CompressionAlgorithm::BROTLI, CompressionAlgorithm::LZMA
    };

    for (auto algo : compressionAlgos) {
        std::string str = std::string(CompressionAlgorithmToString(algo));
        auto converted = StringToCompressionAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << "     " << str << ": " << (success ? "✅" : "❌") << std::endl;
    }

    // 测试ChecksumAlgorithm转换
    std::cout << "  🔐 ChecksumAlgorithm 转换测试:" << std::endl;
    std::vector<ChecksumAlgorithm> checksumAlgos = {
        ChecksumAlgorithm::NONE, ChecksumAlgorithm::CRC32,
        ChecksumAlgorithm::MD5, ChecksumAlgorithm::SHA1,
        ChecksumAlgorithm::SHA256, ChecksumAlgorithm::SHA512,
        ChecksumAlgorithm::BLAKE2
    };

    for (auto algo : checksumAlgos) {
        std::string str = std::string(ChecksumAlgorithmToString(algo));
        auto converted = StringToChecksumAlgorithm(str);
        bool success = (converted.has_value() && converted.value() == algo);
        std::cout << "     " << str << ": " << (success ? "✅" : "❌") << std::endl;
    }
}
```

