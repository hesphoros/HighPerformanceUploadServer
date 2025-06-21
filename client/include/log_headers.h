#ifndef INCLUDE_LUSP_LOG_HEADERS_H
#define INCLUDE_LUSP_LOG_HEADERS_H

#if defined(_MSC_VER) && _MSC_VER >= 1600
#pragma once
#pragma execution_character_set("utf-8")
#endif // _MSC_VER >= 1600

#include "log/LightLogWriteImpl.h"

extern LightLogWrite_Impl g_luspLogWriteImpl;
// 日志宏定义
constexpr const char* LOG_INFO    = "[  INFO   ]";
constexpr const char* LOG_ERROR   = "[  ERROR  ]";
constexpr const char* LOG_DEBUG   = "[  DEBUG  ]";
constexpr const char* LOG_WARN    = "[  WARN   ]";
constexpr const char* LOG_FATAL   = "[  FATAL  ]";
constexpr const char* LOG_TRACE   = "[  TRACE  ]";
constexpr const char* LOG_OK      = "[   OK    ]";

void initializeLogging();

#endif // INCLUDE_LUSP_LOG_HEADERS_H