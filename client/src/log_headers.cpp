#include "log_headers.h"
#include "log/UniConv.h"


LightLogWrite_Impl g_luspLogWriteImpl;

LightLogWrite_Impl g_LogSyncUploadQueueInfo;

LightLogWrite_Impl g_LogSyncNotificationService;

LightLogWrite_Impl g_LogAsioLoopbackIpcClient;

void initializeLogging() {
    UniConv::GetInstance()->SetDefaultEncoding("UTF-8");
    g_luspLogWriteImpl.SetLastingsLogs("./log", "UploadClient-");
    g_LogSyncUploadQueueInfo.SetLastingsLogs("./log", "SyncUploadQueue-");
    g_LogSyncNotificationService.SetLastingsLogs("./log", "SyncNotification-");
	g_LogAsioLoopbackIpcClient.SetLastingsLogs("./log", "AsioLoopbackIpcClient-");
}