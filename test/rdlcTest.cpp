#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../my_components/rdlc/rdlc.h"

/**
 *@brief Ӳ���ӿ�
**/
int RdlcGtestVprintf(RdlcLogLevel_t level,const char *fmt,va_list args)
{
    printf("[%d] ",level);
    vprintf(fmt,args);
    printf("\n");
}

/**
 *@brief �ص������ͽӿ�ģ��
**/
class RdlcMockCallback_t
{
    public:
        MOCK_METHOD(int, OnParsed, (Rdlc_t,const uint8_t*,uint16_t));
        //MOCK_METHOD(int,PortVprintf,(RdlcLogLevel_t,const char *,va_list));
};


MATCHER_P2(EqWithMessage, expected, len, "") {
    return std::memcmp(arg, expected, len) == 0;
}

//========================================================================================

/**
 *@brief ����1���߼�����Ҫ�γɱջ���Ҳ���Լ�����������֡�ܱ��Լ�ʶ��
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ReadWriteMock;
}

extern "C" int RdlcTestReadWriteCallback(Rdlc_t handle,const uint8_t* data,uint16_t size)
{
    ReadWriteMock.OnParsed(handle,data, size);
    return 0;
}

TEST(RdlcTestBasic, ReadWrite)
{
    const uint8_t expected[] = { 0x1,0x2,0x3,0x4,0x6,0x6 };

    // ����ʵ��
    static const RdlcConfig_t config = {
        .msgMaxSize = sizeof(expected),
        .msgMaxEscapeSize = 0,
        .cbParsed = RdlcTestReadWriteCallback,
        .cbError = NULL,
    };
    static const RdlcPort_t port = {
        .portMalloc = malloc,
        .portFree = free,
        .portPrintf = RdlcGtestVprintf
    };
    Rdlc_t handle = xRdlcCreate(&config, &port);
    //vRdlcSetLogLevel(handle,RDLC_LOG_DEBUG);
    EXPECT_NE(handle, nullptr) << "rdlc: init handle failed";

    // ����
    uint8_t txBuf[30];
    int len = xRdlcWriteBytes(handle,expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // ����
    EXPECT_CALL(ReadWriteMock, OnParsed(::testing::_,EqWithMessage(expected,sizeof(expected)),sizeof(expected)));
    int err = xRdlcReadBytes(handle,txBuf,len);
    ASSERT_GT(err,RDLC_NOT_FINISH) << "rdlc: read not finish "<< err;

    vRdlcDestroy(handle);
}


//========================================================================================

/**
 *@brief ����2����ӦHAL�����󣬲����ڶ����ְ����Ƿ�����������
**/

extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> PieceMock;
}

extern "C" int RdlcTestPieceCallback(Rdlc_t handle,const uint8_t* data,uint16_t size)
{
    PieceMock.OnParsed(handle,data, size);
    return 0;
}

TEST(RdlcTestBasic, EachTwoRead)
{
    const uint8_t expected[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};

    // ����ʵ��
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

    // ����
    uint8_t txBuf[40];
    int len = xRdlcWriteBytes(handle,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // ��ν��գ�һ��1�ֽ�
    EXPECT_CALL(PieceMock,OnParsed(::testing::_,EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
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
 *@brief ����3����ӦHAL�����󣬲����ܷ������ȳ�����
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ContinueMock;
}

extern "C" int RdlcTestContinueCallback(Rdlc_t handle,const uint8_t* data,uint16_t size)
{
    ContinueMock.OnParsed(handle,data, size);
    return 0;
}

TEST(RdlcTestBasic, ContinueRead)
{
    const uint8_t expected[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};

    // ����ʵ��
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

    // ����
    uint8_t txBuf[40];
    int len = xRdlcWriteBytes(handle,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed";

    // ��ν��գ�һ��1�ֽ�
    EXPECT_CALL(ContinueMock, OnParsed(::testing::_,EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
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
 *@brief ����4����ӦHAL�����󣬲����ܷ��������ȳ�����
**/
extern "C" {
    static ::testing::StrictMock<RdlcMockCallback_t> ContinueVariMock;
}

extern "C" int RdlcTestContinueVariCallback(Rdlc_t handle,const uint8_t* data,uint16_t size)
{
    ContinueVariMock.OnParsed(handle,data, size);
    return 0;
}

TEST(RdlcTestBasic, ContinueVariRead)
{
    const uint8_t expected[]  = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};
    const uint8_t expected2[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8};

    // ����ʵ��
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

    // ����
    uint8_t txBuf1[40];
    uint8_t txBuf2[40];
    int len1 = xRdlcWriteBytes(handle,(const uint8_t*)expected,sizeof(expected),txBuf1,sizeof(txBuf1));
    ASSERT_GT(len1,RDLC_OK) << "rdlc: write failed 1";
    int len2 = xRdlcWriteBytes(handle,(const uint8_t*)expected2,sizeof(expected2),txBuf2,sizeof(txBuf2));
    ASSERT_GT(len2,RDLC_OK) << "rdlc: write failed 2";

    // ��ν���
    EXPECT_CALL(ContinueVariMock, OnParsed(::testing::_,EqWithMessage(expected,sizeof(expected)),sizeof(expected)))
        .Times(1)
        .WillOnce(::testing::Return(0));
    EXPECT_CALL(ContinueVariMock, OnParsed(::testing::_,EqWithMessage(expected2,sizeof(expected2)),sizeof(expected2)))
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
 *@brief ����5���ٽ�д�����
**/
TEST(RdlcTestCritical, WriteFail)
{
    const uint8_t expected[]  = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD};

    // ����ʵ��
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

    // ��һ������
    len = xRdlcWriteBytes(handle,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed less 1";

    // ǡ�÷���
    int len2 = xRdlcWriteBytes(handle,(const uint8_t*)expected,sizeof(expected),txBuf,sizeof(txBuf));
    ASSERT_GT(len,RDLC_OK) << "rdlc: write failed 2";

    // ��һ������

    vRdlcDestroy(handle);
}





//todo��ת���ַ���Ŀ��������֡ͷ�������е�֡ͷ�����0xC0Ӧ������Ϊ��������������0xFF 0xC0


