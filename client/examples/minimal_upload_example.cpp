/**
 * @file 极简上传调用示例
 * @brief 演示如何使用新的极简架构上传文件
 * 
 * 🎯 核心理念：UI线程只需要一行代码 - queue.push() 就完事！
 */

#include "SyncUploadQueue/Lusp_SyncUploadQueue.h"
#include <iostream>
#include <vector>
#include <string>

/**
 * 🚀 示例1：控制台应用的极简调用
 */
void example_console_upload() {
    std::cout << "=== 控制台极简上传示例 ===" << std::endl;
    
    // 🎯 就这么简单！UI只需要一行代码！
    Upload::push("C:/test/document.pdf");
    Upload::push("C:/test/image.jpg");
    
    // 或者批量上传
    std::vector<std::string> files = {
        "C:/test/video.mp4",
        "C:/test/archive.zip",
        "C:/test/readme.txt"
    };
    Upload::push(files);
    
    std::cout << "✅ 文件已提交到极简上传队列！" << std::endl;
    std::cout << "剩下的全部由通知线程和本地服务自动处理" << std::endl;
}

/**
 * 🚀 示例2：Qt应用的极简调用（参考MainWindow实现）
 */
void example_qt_upload() {
    std::cout << "=== Qt GUI极简上传示例 ===" << std::endl;
    
    // 用户拖拽文件或点击选择后，UI线程只需要：
    QStringList qtFilePaths = {"C:/uploads/file1.txt", "C:/uploads/file2.png"};
    
    // 转换为std::vector<std::string>
    std::vector<std::string> filePaths;
    for (const QString& path : qtFilePaths) {
        filePaths.push_back(path.toStdString());
    }
    
    // 🎯 UI线程的全部工作就是这一行！
    Lusp_SyncUploadQueue::instance().push(filePaths);
    
    std::cout << "✅ Qt UI已完成工作！文件已入队！" << std::endl;
    // UI可以立即返回，继续响应用户操作
    // 通知线程会自动处理后续一切
}

/**
 * 🚀 示例3：设置可选的进度回调
 */
void example_with_progress_callback() {
    std::cout << "=== 带进度回调的极简上传示例 ===" << std::endl;
    
    // 可选：设置进度回调（UI可以选择显示或不显示）
    Lusp_SyncUploadQueue::instance().setProgressCallback(
        [](const std::string& filePath, int percentage, const std::string& status) {
            std::cout << "📊 上传进度: " << filePath 
                      << " - " << percentage << "% - " << status << std::endl;
        }
    );
    
    // 可选：设置完成回调
    Lusp_SyncUploadQueue::instance().setCompletedCallback(
        [](const std::string& filePath, bool success, const std::string& message) {
            if (success) {
                std::cout << "✅ 上传成功: " << filePath << std::endl;
            } else {
                std::cout << "❌ 上传失败: " << filePath << " - " << message << std::endl;
            }
        }
    );
    
    // 🎯 设置完回调后，正常使用极简接口
    Upload::push("C:/important/data.xlsx");
    
    std::cout << "✅ 文件已提交，进度会自动回调显示" << std::endl;
}

/**
 * 🚀 示例4：检查队列状态（可选）
 */
void example_queue_status() {
    std::cout << "=== 队列状态查询示例 ===" << std::endl;
    
    auto& queue = Lusp_SyncUploadQueue::instance();
    
    // 添加一些文件
    Upload::push("C:/status/file1.txt");
    Upload::push("C:/status/file2.txt");
    
    // 查询队列状态（这些都是无锁原子操作）
    std::cout << "📈 队列中待处理文件数: " << queue.pendingCount() << std::endl;
    std::cout << "🔄 通知线程是否活跃: " << (queue.isActive() ? "是" : "否") << std::endl;
    std::cout << "📭 队列是否为空: " << (queue.empty() ? "是" : "否") << std::endl;
}

/**
 * 📋 使用总结
 */
void usage_summary() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "🎯 极简架构使用总结" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    std::cout << "✨ UI线程的工作（极简）：" << std::endl;
    std::cout << "   1. 检测到需要上传的文件" << std::endl;
    std::cout << "   2. 调用 Upload::push(filePath) 或 queue.push()" << std::endl;
    std::cout << "   3. 立即返回，继续响应用户操作" << std::endl;
    std::cout << "   4. 完事！不需要关心任何上传细节" << std::endl;
    
    std::cout << "\n🧵 后台自动处理（透明）：" << std::endl;
    std::cout << "   • 通知线程：条件变量等待 → 给本地服务发通知" << std::endl;
    std::cout << "   • 本地服务：接收通知 → 后台上传 → 进度回调" << std::endl;
    std::cout << "   • UI回调：显示进度（可选）→ 用户看到结果" << std::endl;
    
    std::cout << "\n🏗️ 架构优势：" << std::endl;
    std::cout << "   ✅ UI极简：只需一行代码" << std::endl;
    std::cout << "   ✅ 高并发：行级锁队列" << std::endl;
    std::cout << "   ✅ 职责清晰：分层独立" << std::endl;
    std::cout << "   ✅ 易维护：标准C++实现" << std::endl;
    std::cout << "   ✅ 可扩展：支持任何UI框架" << std::endl;
}

int main() {
    std::cout << "🚀 高性能文件上传客户端 - 极简架构演示" << std::endl;
    std::cout << "基于 client.md 设计的真实极简分层架构\n" << std::endl;
    
    example_console_upload();
    std::cout << std::endl;
    
    example_qt_upload();
    std::cout << std::endl;
    
    example_with_progress_callback();
    std::cout << std::endl;
    
    example_queue_status();
    
    usage_summary();
    
    std::cout << "\n🎉 演示完成！程序运行中，通知线程正在后台工作..." << std::endl;
    
    // 保持程序运行，观察通知线程工作
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    return 0;
}
