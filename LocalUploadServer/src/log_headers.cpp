#include "log_headers.h"

LightLogWrite_Impl g_LogAsioLoopbackIpcServer;

void initializeLogger()
{
	g_LogAsioLoopbackIpcServer.SetLastingsLogs(L"logs", L"AsioLoopbackIpcServer-");
}
