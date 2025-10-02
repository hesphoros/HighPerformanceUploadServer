// 测试编码转换
#include <iostream>
#include <string>
#include <UniConv.h>

int main() {
    // 设置默认编码为UTF-8
    UniConv::GetInstance()->SetDefaultEncoding("UTF-8");

    std::cout << "当前系统编码: " << UniConv::GetInstance()->GetCurrentSystemEncoding() << std::endl;

    // 测试简单的ASCII字符串
    std::string test1 = "./log";
    std::cout << "测试字符串1: " << test1 << std::endl;

    auto result = UniConv::GetInstance()->LocaleConvertToUcs4(test1);
    if (result.empty()) {
        std::cerr << "转换失败！" << std::endl;
    }
    else {
        std::wcout << L"转换成功: " << result << std::endl;
    }

    return 0;
}
