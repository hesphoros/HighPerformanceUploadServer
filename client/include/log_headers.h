#ifndef INCLUDE_LUSP_LOG_HEADERS_H
#define INCLUDE_LUSP_LOG_HEADERS_H

#if defined(_MSC_VER) && _MSC_VER >= 1600
#pragma once
#pragma execution_character_set("utf-8")
#endif // _MSC_VER >= 1600

#include "log/LightLogWriteImpl.h"
#include "log/UniConv.h"

extern LightLogWrite_Impl g_luspLogWriteImpl;
extern LightLogWrite_Impl g_LogSyncUploadQueueInfo;

#define LUSP_UNICONV   UniConv::GetInstance()

// 日志宏定义
constexpr const char* LOG_INFO    = "[  INFO   ]";
constexpr const char* LOG_ERROR   = "[  ERROR  ]";
constexpr const char* LOG_DEBUG   = "[  DEBUG  ]";
constexpr const char* LOG_WARN    = "[  WARN   ]";
constexpr const char* LOG_FATAL   = "[  FATAL  ]";
constexpr const char* LOG_TRACE   = "[  TRACE  ]";
constexpr const char* LOG_OK      = "[   OK    ]";


constexpr const char16_t* LOG_INFO_U16    = u"[  INFO   ]";
constexpr const char16_t* LOG_ERROR_U16   = u"[  ERROR  ]";
constexpr const char16_t* LOG_DEBUG_U16   = u"[  DEBUG  ]";
constexpr const char16_t* LOG_WARN_U16    = u"[  WARN   ]";
constexpr const char16_t* LOG_FATAL_U16   = u"[  FATAL  ]";
constexpr const char16_t* LOG_TRACE_U16   = u"[  TRACE  ]";
constexpr const char16_t* LOG_OK_U16      = u"[   OK    ]";


void initializeLogging();

#endif // INCLUDE_LUSP_LOG_HEADERS_H
