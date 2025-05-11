#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/// 配置宏
#define RDLC_CRC16_USE_CALCULATE  0
#define RDLC_CRC16_USE_TABLE      1

/// 日志层次
typedef enum{
    RDLC_LOG_DEBUG = 0,
    RDLC_LOG_INFO  = 1,
    RDLC_LOG_WARN  = 2,
    RDLC_LOG_ERR   = 3,
    RDLC_LOG_NONE  = 4,
}RdlcLogLevel_t;

/// 错误码
#define RDLC_OK 0
#define RDLC_NOT_FINISH -1
#define RDLC_ERR_NOT_ALLOWED -2
#define RDLC_ERR_CRC -3
#define RDLC_ERR_INVALID_ARG -4
#define RDLC_ERR_BUFFER_TOO_SHORT -5

// 转义状态
#define RDLC_STATE_ESCAPE_WAIT 0 ///< 无需转义
#define RDLC_STATE_ESCAPE_GET  1 ///< 等待转义
// 解析状态
#define RDLC_STATE_PARSE_WAIT_HEAD 0   ///< 等待帧头
#define RDLC_STATE_PARSE_GET_CTRL 1    ///< 等待控制字段
#define RDLC_STATE_PARSE_GET_LENL 2    ///< 等待载荷长度低八位
#define RDLC_STATE_PARSE_GET_LENH 3    ///< 等待载荷长度高八位
#define RDLC_STATE_PARSE_GET_PAYLOAD 4 ///< 等待载荷
#define RDLC_STATE_PARSE_GET_CRCL 5    ///< 等待校验码低八位
#define RDLC_STATE_PARSE_GET_CRCH 6    ///< 等待校验码高八位
#define RDLC_STATE_PARSE_GET_TAIL 7    ///< 等待帧尾

// 底层接口定义
typedef void* (*RdlcMalloc_fptr)(size_t);
typedef void  (*RdlcFree_fptr)  (void*);
typedef int   (*RdlcPrintf_fptr)(RdlcLogLevel_t level,const char *fmt,va_list args);

// 基本接口类型定义
typedef void* Rdlc_t;
typedef int (*RdlcOnParse_fptr) (Rdlc_t,const uint8_t*,uint16_t);
typedef int (*RdlcOnError_fptr) (Rdlc_t,int);

/// 配置类型
typedef struct{
    uint16_t msgMaxSize;
    uint16_t msgMaxEscapeSize;
    RdlcOnParse_fptr cbParsed;
    RdlcOnError_fptr cbError;
}RdlcConfig_t;

/// 接口类型
typedef struct{
    RdlcMalloc_fptr portMalloc;
    RdlcFree_fptr portFree;
    RdlcPrintf_fptr portPrintf;
}RdlcPort_t;

// 构造函数和析构函数
Rdlc_t xRdlcCreate(const RdlcConfig_t *config,const RdlcPort_t *port);
void   vRdlcDestroy(Rdlc_t protoHandle);

// 对象方法1：解包
int xRdlcReadByte(Rdlc_t protoHandle,uint8_t byte);
int xRdlcReadBytes(Rdlc_t protoHandle,uint8_t *buffer,uint16_t size);

// 对象方法2：封包
int xRdlcWriteBytes(Rdlc_t protoHandle,const uint8_t *payload,uint16_t payloadSize,
                    uint8_t *frameBuf,uint16_t frameMaxSize);

// 对象方法3：流控
int xRdlcReset(Rdlc_t protoHandle);

// 对象方法4：工作状态
int xRdlcGetParseState(Rdlc_t protoHandle);
int xRdlcGetEscapeState(Rdlc_t protoHandle);

// 对象成员1：日志层次
RdlcLogLevel_t xRdlcGetLogLevel(Rdlc_t protoHandle);
void vRdlcSetLogLevel(Rdlc_t protoHandle,RdlcLogLevel_t level);


#ifdef __cplusplus
}
#endif
