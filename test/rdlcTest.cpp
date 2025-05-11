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
 *@brief 测试1：逻辑至少要形成闭环，也即自己创建出来的帧能被自己识别
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ReadWriteMock;
}

extern "C" int RdlcTestReadWriteCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    printf("Callback is triggered\n");
    ReadWriteMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, ReadWrite)
{
    const uint8_t expected[] = { 0x1,0x2,0x3,0x4,0x6,0x6 };
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建动态实例
    RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 0,
        .cbParsed = RdlcTestReadWriteCallback,
        .cbError = NULL,
    };
    RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf
    };
    Rdlc_t handle = xRdlcCreate(&config, &port);
    vRdlcSetLogLevel(handle,RDLC_LOG_DEBUG);
    EXPECT_NE(handle, nullptr) << "rdlc: init handle failed";

    // 发送
    uint8_t txBuf[30];
    int len = xRdlcWriteBytes(handle,expectAddr,expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // 接收
    EXPECT_CALL(ReadWriteMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)));
    int err = xRdlcReadBytes(handle,txBuf,len);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish "<< err;

    vRdlcDestroy(handle);

    // 创建静态实例
    static RdlcStaticHandle_t staticHandle;
    RdlcPort_t portStatic = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf,
    };

    static uint8_t staticRxBuffer[100];
    Rdlc_t sHandle = xRdlcCreateStatic(&config,&portStatic,&staticHandle,staticRxBuffer,sizeof(staticRxBuffer));
    EXPECT_NE(sHandle, nullptr) << "rdlc: init handle failed";
    vRdlcSetLogLevel(sHandle,RDLC_LOG_DEBUG);

    // 发送
    uint8_t txBuf2[30];
    len = xRdlcWriteBytes(sHandle,expectAddr,expected,sizeof(expected),txBuf2,sizeof(txBuf2));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // 接收
    EXPECT_CALL(ReadWriteMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)));
    err = xRdlcReadBytes(sHandle,txBuf2,len);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish "<< err;
}


//========================================================================================

/**
 *@brief 测试2：对应HAL库需求，测试在定长分包后是否能正常解析
**/

extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> PieceMock;
}

extern "C" int RdlcTestPieceCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    PieceMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, EachTwoRead)
{
    const uint8_t expected[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 3,
        .cbParsed = RdlcTestPieceCallback,
        .cbError = NULL,
    };
    static const RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf
    };
    Rdlc_t handle = xRdlcCreate(&config, &port);
    EXPECT_NE(handle, nullptr) << "rdlc: init handle failed";
    //vRdlcSetLogLevel(handle,RDLC_LOG_DEBUG);

    // 发送
    uint8_t txBuf[40];
    int len = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // 多次接收，一次1字节
    EXPECT_CALL(PieceMock,OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
        .Times(1)
        .WillOnce(::testing::Return(0));

    int index = 0;
    int err;
    for(int i=0;i<len;i++)
        if (i!=len-1) {
            err = xRdlcReadByte(handle,txBuf[i]);
            ASSERT_EQ(err,RDLC_NOT_FINISH)<< "rdlc: read finish too early,code="<< err;
        }
        else {
            err = xRdlcReadByte(handle,txBuf[i]);
            ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish cod ="<< err;
        }

    vRdlcDestroy(handle);
}
//========================================================================================

/**
 *@brief 测试3：对应HAL库需求，测试能否连续等长解析
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ContinueMock;
}

extern "C" int RdlcTestContinueCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    ContinueMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, ContinueRead)
{
    const uint8_t expected[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 3,
        .cbParsed = RdlcTestContinueCallback,
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

    // 发送
    uint8_t txBuf[40];
    int len = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // 多次接收，一次1字节
    EXPECT_CALL(ContinueMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
        .Times(2)
        .WillOnce(::testing::Return(0));

    int index = 0;
    int err;

    for(int i=0;i<len;i++)
        if (i!=len-1) {
            err = xRdlcReadByte(handle,txBuf[i]);
            ASSERT_EQ(err,RDLC_NOT_FINISH)<< "rdlc: read finish too early,code="<< err;
        }
        else {
            err = xRdlcReadByte(handle,txBuf[i]);
            ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish code="<< err;
        }

    err = xRdlcReadBytes(handle,txBuf,len);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish code="<< err;

    vRdlcDestroy(handle);
}

//========================================================================================

/**
 *@brief 测试4：对应HAL库需求，测试能否连续不等长解析
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ContinueVariMock;
}

extern "C" int RdlcTestContinueVariCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    ContinueVariMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, ContinueVariRead)
{
    const uint8_t expected[]  = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const uint8_t expected2[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8};
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 3,
        .cbParsed = RdlcTestContinueVariCallback,
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

    // 发送
    uint8_t txBuf1[40];
    uint8_t txBuf2[40];
    int len1 = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected,sizeof(expected),txBuf1,sizeof(txBuf1));
    ASSERT_GT(len1,RDLC_OK) << "rdlc: write failed 1";
    int len2 = xRdlcWriteBytes(handle,expectAddr,(const uint8_t*)expected2,sizeof(expected2),txBuf2,sizeof(txBuf2));
    ASSERT_GT(len2,RDLC_OK) << "rdlc: write failed 2";

    // 多次接收
    EXPECT_CALL(ContinueVariMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
        .Times(1)
        .WillOnce(::testing::Return(0));
    EXPECT_CALL(ContinueVariMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected2,sizeof(expected2)),sizeof(expected2)))
        .Times(1)
        .WillOnce(::testing::Return(0));

    int index = 0;
    int err;

    err = xRdlcReadBytes(handle,txBuf1,len1);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read 1 not finish code="<< err;
    err = xRdlcReadBytes(handle,txBuf2,len2);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read 2 not finish code="<< err;

    vRdlcDestroy(handle);
}

//========================================================================================

/**
 *@brief 测试5：共用信道传输数据
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ParellMock;
}

extern "C" int RdlcTestParellCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    ParellMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, ParellRead)
{
    const uint8_t expected[]  = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const uint8_t expected2[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8};
    const RdlcAddr_t expectAddr1 = {.srcAddr = 0x01, .dstAddr = 0x02};
    const RdlcAddr_t expectAddr2 = {.srcAddr = 0x01, .dstAddr = 0xFF};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 3,
        .cbParsed = RdlcTestParellCallback,
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

    // 发送
    uint8_t txBuf1[40];
    uint8_t txBuf2[40];
    int len1 = xRdlcWriteBytes(handle,expectAddr1,(const uint8_t*)expected,sizeof(expected),txBuf1,sizeof(txBuf1));
    ASSERT_GT(len1,RDLC_OK) << "rdlc: write failed 1";
    int len2 = xRdlcWriteBytes(handle,expectAddr2,(const uint8_t*)expected2,sizeof(expected2),txBuf2,sizeof(txBuf2));
    ASSERT_GT(len2,RDLC_OK) << "rdlc: write failed 2";

    // 多次接收
    EXPECT_CALL(ParellMock, OnParsed(::testing::_,AddrEq(expectAddr1.srcAddr,expectAddr1.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
        .Times(1)
        .WillOnce(::testing::Return(0));
    EXPECT_CALL(ParellMock, OnParsed(::testing::_,AddrEq(expectAddr2.srcAddr,expectAddr2.dstAddr),EqWithMessage(expected2,sizeof(expected2)),sizeof(expected2)))
        .Times(1)
        .WillOnce(::testing::Return(0));

    int index = 0;
    int err;

    err = xRdlcReadBytes(handle,txBuf1,len1);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read 1 not finish code="<< err;
    err = xRdlcReadBytes(handle,txBuf2,len2);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read 2 not finish code="<< err;

    vRdlcDestroy(handle);
}


//========================================================================================

/**
 *@brief 测试6：断包自动排除
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> SyncMock;
}

extern "C" int RdlcSyncCallback(Rdlc_t handle,RdlcAddr_t addr,const uint8_t* data,uint16_t size)
{
    SyncMock.OnParsed(handle,addr,data,size);
    return 0;
}

TEST(RdlcTestBasic, SyncRead)
{
    const uint8_t expected[] = { 0x1,0x2,0x3,0x4,0x6,0x6 };
    const RdlcAddr_t expectAddr = {.srcAddr = 0x01, .dstAddr = 0x02};

    // 创建实例
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 0,
        .cbParsed = RdlcSyncCallback,
        .cbError = NULL,
    };
    static const RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf
    };
    Rdlc_t handle = xRdlcCreate(&config, &port);
    vRdlcSetLogLevel(handle,RDLC_LOG_DEBUG);
    EXPECT_NE(handle, nullptr) << "rdlc: init handle failed";

    // 发送
    uint8_t txBuf[30];
    int len = xRdlcWriteBytes(handle,expectAddr,expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // 接收不完整
    EXPECT_CALL(SyncMock, OnParsed(::testing::_,AddrEq(expectAddr.srcAddr,expectAddr.dstAddr),EqWithMessage(expected,sizeof(expected)),sizeof(expected)));
    int err = xRdlcReadBytes(handle,&txBuf[1],len-1);
    ASSERT_EQ(err,RDLC_NOT_FINISH) << "rdlc: accidentally finish read"<< err;

    // 接收完整
    err = xRdlcReadBytes(handle,txBuf,len);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish "<< err;

    vRdlcDestroy(handle);
}





