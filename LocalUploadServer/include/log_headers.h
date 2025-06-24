#ifndef INCLUDE_LOG_HEADERS_H_
#define INCLUDE_LOG_HEADERS_H_

#include "log/LightLogWriteImpl.h"
#include "log/UniConv.h"
#include "tabulate/tabulate.hpp"

extern LightLogWrite_Impl g_LogAsioLoopbackIpcServer;

constexpr const char* LOG_INFO = "[  INFO   ]";
constexpr const char* LOG_ERROR = "[  ERROR  ]";
constexpr const char* LOG_DEBUG = "[  DEBUG  ]";
constexpr const char* LOG_WARN = "[  WARN   ]";
constexpr const char* LOG_FATAL = "[  FATAL  ]";
constexpr const char* LOG_TRACE = "[  TRACE  ]";
constexpr const char* LOG_OK = "[   OK    ]";

void initializeLogger();

#define LUSP_UNICONV UniConv::GetInstance()

#endif // !INCLUDE_LOG_HEADERS_H_