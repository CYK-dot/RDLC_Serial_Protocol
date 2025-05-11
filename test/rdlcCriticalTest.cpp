#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../my_components/rdlc/rdlc.h"
#include "rdlcTestPrivate.h"

/**
 *@brief 硬件接口
**/
static int RdlcGtestVprintf(RdlcLogLevel_t level,const char *fmt,va_list args)
{
    printf("[%d] ",level);
    vprintf(fmt,args);
    printf("\n");
}


//========================================================================================

/**
 *@brief 异常测试1：临界写入测试
**/
TEST(RdlcTestCritical, WriteFail)
{
    const uint8_t expected[]  = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 3,
        .cbParsed = NULL,
        .cbError = NULL,
    };
    static const RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf
    };
    Rdlc_t handle = xRdlcCreate(&config, &port);
    EXPECT_NE(handle, nullptr) << "rdlc: init handle failed";
    vRdlcSetLogLevel(handle,RDLC_LOG_DEBUG);

    uint8_t txBuf[40];
    int len;

    // 少一个发送
    len = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed less 1";

    // 恰好发送
    int len2 = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed 2";

    // 多一个发送

    vRdlcDestroy(handle);
}
