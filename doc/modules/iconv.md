~~~c

    /// <summary>
    /// UCS4转本地编码
    /// </summary>
    /// <param name="sInput"></param>
    /// <returns></returns>
    std::string Lcs_CharConverted::Ucs4ConversLocale(const std::wstring& sInput)
    {
        if (sInput.empty())return "";

        std::string sLocaleCode = GetCurrSysCharset();
        if (sLocaleCode.empty())return Ucs4ConvertToUtf8(sInput);

        iconv_t iConvertObj;
#if defined(_WIN32) || defined(_WIN64)
        iConvertObj = iconv_open(sLocaleCode.c_str(), "UTF-16LE"); // 在Windows上，wstring是UTF-16
#else
        iConvertObj = iconv_open(sLocaleCode.c_str(), "UTF-32LE"); // 在Unix/Linux上，wstring是UTF-32
#endif

        if (iConvertObj == (iconv_t)-1) return "";

        size_t sInputBytes = sInput.size() * sizeof(std::wstring::value_type);
        size_t sOutputByte = sInputBytes * 4; // 最多需要4倍的空间来存储UTF-8字符串
        std::vector<char> vOutsBuffer(sOutputByte);
        const char* cInputsPtr = reinterpret_cast<char*>(const_cast<wchar_t*>(sInput.data()));
        char* cOutputPtr = vOutsBuffer.data();

        size_t sResultSize = iconv(iConvertObj, &cInputsPtr, &sInputBytes, &cOutputPtr, &sOutputByte);
        if (sResultSize == (size_t)-1) {
            iconv_close(iConvertObj);
            return "";
        }

        iconv_close(iConvertObj);
        return std::string(vOutsBuffer.data(), vOutsBuffer.size() - sOutputByte);
    }

    std::string Lcs_CharConverted::Ucs4ConversLocale(const wchar_t* sInput)
    {
        std::wstring ssInput = sInput;
        return Ucs4ConversLocale(ssInput);
    }
~~~

~~~c

    /// <summary>
    /// utf8转本地编码
    /// </summary>
    /// <param name="sInput"></param>
    /// <returns></returns>
    std::string Lcs_CharConverted::Utf8ConvertLocale(const std::string& sInput)
    {
        if (sInput.empty())return "";

        std::string sLocaleCode = GetCurrSysCharset();
        if (sLocaleCode.empty())return sInput;

        iconv_t iConvertObj = iconv_open(sLocaleCode.c_str(), "UTF-8");
        if (iConvertObj == (iconv_t)-1) return "";

        size_t sInputBytes = sInput.size() * sizeof(std::string::value_type);
        size_t sOutputByte = sInputBytes * 3; 

        std::vector<char> vOutsBuffer(sOutputByte);

        const char* cInputsPtr = reinterpret_cast<char*>(const_cast<char*>(sInput.data()));
        char* cOutputPtr = vOutsBuffer.data();

        size_t sResultSize = iconv(iConvertObj, &cInputsPtr, &sInputBytes, &cOutputPtr, &sOutputByte);
        if (sResultSize == (size_t)-1) {
            iconv_close(iConvertObj);
            return "";
        }

        iconv_close(iConvertObj);
        return std::string(vOutsBuffer.data(), vOutsBuffer.size() - sOutputByte);
    }

    std::string Lcs_CharConverted::Utf8ConvertLocale(const char* sInput)
    {
        std::string ssInput = sInput;
        return Utf8ConvertLocale(ssInput);
    }
~~~

~~~c
    /// <summary>
    /// 本地编码转utf8
    /// </summary>
    /// <param name="sInput"></param>
    /// <returns></returns>
    std::string Lcs_CharConverted::LocaleConvertUtf8(const std::string& sInput)
    {
        if (sInput.empty())return "";

        std::string sLocaleCode = GetCurrSysCharset();
        if (sLocaleCode.empty())return sInput;

        iconv_t iConvertObj = iconv_open("UTF-8", sLocaleCode.c_str());
        if (iConvertObj == (iconv_t)-1) return "";

        size_t sInputBytes = sInput.size() * sizeof(std::string::value_type);
        size_t sOutputByte = sInputBytes * 3; 

        std::vector<char> vOutsBuffer(sOutputByte);

        const char* cInputsPtr = reinterpret_cast<char*>(const_cast<char*>(sInput.data()));
        char* cOutputPtr = vOutsBuffer.data();

        size_t sResultSize = iconv(iConvertObj, &cInputsPtr, &sInputBytes, &cOutputPtr, &sOutputByte);
        if (sResultSize == (size_t)-1) {
            iconv_close(iConvertObj);
            return "";
        }

        iconv_close(iConvertObj);
        return std::string(vOutsBuffer.data(), vOutsBuffer.size() - sOutputByte);
    }

    std::string Lcs_CharConverted::LocaleConvertUtf8(const char* sInput)
    {
        std::string ssInput = sInput;
        r
~~~

~~~c
/// <summary>
/// 本地编码转UTF16
/// </summary>
/// <param name="sInput"></param>
/// <returns></returns>
std::u16string Lcs_CharConverted::LocaleConvertsU16(const std::string& sInput)
{
    if (sInput.empty()) return u"";

    std::string sLocaleCode = GetCurrSysCharset();
    if (sLocaleCode.empty())return Utf8ConvertsUtf16(sInput);

    iconv_t iConvertObj = iconv_open("UTF-16LE", sLocaleCode.c_str());
    if (iConvertObj == (iconv_t)-1)return u"";

    size_t sInputBytes = sInput.size();
    size_t sOutputByte = sInputBytes * 2; // UTF-16可能需要比UTF-8更多的字节
    std::vector<char> vOutsBuffer(sOutputByte);
    const char* cInputsPtr = const_cast<char*>(sInput.data());
    char* cOutputPtr = vOutsBuffer.data();

    size_t result = iconv(iConvertObj, &cInputsPtr, &sInputBytes, &cOutputPtr, &sOutputByte);
    if (result == (size_t)-1) {
        iconv_close(iConvertObj);
        return u"";
    }

    iconv_close(iConvertObj);
    // 将输出缓冲区转换为std::u16string
    return std::u16string(reinterpret_cast<char16_t*>(vOutsBuffer.data()), (vOutsBuffer.size() - sOutputByte) / sizeof(char16_t));
}

std::u16string Lcs_CharConverted::LocaleConvertsU16(const char* sInput)
{
    std::string ssInput = sInput;
    return LocaleConvertsU16(ssInput);
}
~~~

~~~c

/// <summary>
/// UTF16转本地编码
/// </summary>
/// <param name="sInput"></param>
/// <returns></returns>
std::string Lcs_CharConverted::U16ConvertsLocale(const std::u16string& sInput)
{
    if (sInput.empty())return "";
    std::string sLocaleCode = GetCurrSysCharset();
    if (sLocaleCode.empty())return Utf16ConvertsUtf8(sInput);

    iconv_t iConvertObj = iconv_open(sLocaleCode.c_str(), "UTF-16LE");
    if (iConvertObj == (iconv_t)-1) return "";

    size_t sInputBytes = sInput.size() * sizeof(std::u16string::value_type);
    size_t sOutputByte = sInputBytes * 2; 

    std::vector<char> vOutsBuffer(sOutputByte);

    const char* cInputsPtr = reinterpret_cast<char*>(const_cast<char16_t*>(sInput.data()));
    char* cOutputPtr = vOutsBuffer.data();

    size_t sResultSize = iconv(iConvertObj, &cInputsPtr, &sInputBytes, &cOutputPtr, &sOutputByte);
    if (sResultSize == (size_t)-1) {
        iconv_close(iConvertObj);
        return "";
    }

    iconv_close(iConvertObj);
    return std::string(vOutsBuffer.data(), vOutsBuffer.size() - sOutputByte);
}

std::string Lcs_CharConverted::U16ConvertsLocale(const char16_t* sInput)
{
    std::u16string ssInput = sInput;
    return U16ConvertsLocale(ssInput);
}
~~~

