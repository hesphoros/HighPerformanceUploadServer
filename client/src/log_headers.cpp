#include "log_headers.h"
#include "log/UniConv.h"


LightLogWrite_Impl g_luspLogWriteImpl;



void initializeLogging() {
    UniConv::GetInstance()->SetDefaultEncoding("UTF-8");
    g_luspLogWriteImpl.SetLastingsLogs("./log", "UploadClient-");
}