#ifndef INCLUDE_LUSP_LOG_HEADERS_H
#define INCLUDE_LUSP_LOG_HEADERS_H

#if defined(_MSC_VER) && _MSC_VER >= 1600
#pragma once
#pragma execution_character_set("utf-8")
#endif // _MSC_VER >= 1600

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include "log/LoggerWrapper.h"
#include "UniConv.h"

#define LUSP_UNICONV   UniConv::GetInstance()

// 使用 LoggerWrapper 替代原来的 LightLogWrite_Impl
extern LoggerWrapper g_luspLogWriteImpl;
extern LoggerWrapper g_LogSyncUploadQueueInfo;
extern LoggerWrapper g_LogSyncNotificationService;
extern LoggerWrapper g_LogAsioLoopbackIpcClient;

// 日志级别常量
constexpr const char* LOG_INFO = "INFO";
constexpr const char* LOG_ERROR = "ERROR";
constexpr const char* LOG_DEBUG = "DEBUG";
constexpr const char* LOG_WARN = "WARN";
constexpr const char* LOG_FATAL = "FATAL";
constexpr const char* LOG_TRACE = "TRACE";
constexpr const char* LOG_OK = "INFO";

void initializeLogging();
void shutdownLogging();

#endif // INCLUDE_LUSP_LOG_HEADERS_H
