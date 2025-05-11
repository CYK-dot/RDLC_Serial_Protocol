/**
 * @file rdlc.c
 * @brief RDLC是一款使用面向对象思想封装的，跨平台、线程安全、带日志支持的字节流协议，改进自HDLC协议
 * @author 陈煜楷
 *
 * RDLC是一种基于字节流的协议，含差错控制和字符转义，并采用冗余越界保护，可满足机器人比赛的中高速通信(>=115.2kb/s)需求。
**/

#include "rdlc.h"
#include <string.h>
#include <stdarg.h>


#define BYTE_ESCAPE 0xFF /// 转义字符
#define BYTE_HEAD   0xC0 /// 包头
#define BYTE_TAIL   0x0C /// 包尾


#if (RDLC_CRC16_USE_CALCULATE == 1) && (RDLC_CRC16_USE_TABLE == 1)
    #error "RDLC: you can only choose one of the crc16 methods."
#elif (RDLC_CRC16_USE_CALCULATE == 0) && (RDLC_CRC16_USE_TABLE == 0)
    #error "RDLC: You must define one CRC16 method."
#endif

/**
 *@brief RDLC对象的private成员
**/
typedef struct{
    uint8_t stateParse;
    uint8_t stateEscape;

    uint8_t *rxBuf;
    uint16_t rxBufSize;

    uint16_t rxIndexer;
    uint16_t payloadSize;

    uint16_t payloadMaxSize;
    uint16_t payloadMaxEscapeSize;

    RdlcOnParse_fptr cbParsed;
    RdlcOnError_fptr cbError;
    RdlcPort_t port;
    RdlcLogLevel_t logLevel;
}RdlcStaticHandle_t;

/**
 *@brief 日志调用
 *@addtogroup 支撑功能
**/
#if RDLC_LOG_ENABLE == 1
static inline void Log(RdlcStaticHandle_t *handle,RdlcLogLevel_t level,const char *fmt, ...)
{
    if (handle == NULL || handle->port.portPrintf == NULL)
        return;
    va_list args;
    va_start(args, fmt);
    if (level >= handle->logLevel)
        handle->port.portPrintf(level,fmt,args);
    va_end(args);
}
#else
#define Log(...)  ((void)0)
#endif
/**
 *@brief CRC16(0xA001)计算
 *@addtogroup 支撑功能
**/
#if RDLC_CRC16_USE_CALCULATE == 1
static inline uint16_t prvGetCrc16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}
#elif RDLC_CRC16_USE_TABLE == 1
static const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};
static inline uint16_t prvGetCrc16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint8_t table_index = crc ^ data[i];
        crc = (crc >> 8) ^ crc16_table[table_index];
    }
    return crc;
}
#endif

/**
 *@brief 复位接收缓冲区
 *@addtogroup 接收缓冲区操作
**/
static inline void prvRxBufferReset(RdlcStaticHandle_t *handle)
{
    handle->rxIndexer = 0;
    handle->payloadSize = 0;
}
/**
 *@brief 安全写入接收缓冲区
 *@addtogroup 接收缓冲区操作
**/
static inline int prvRxBufferFeed(RdlcStaticHandle_t *handle,uint8_t data)
{
    if (handle->rxIndexer == handle->rxBufSize) {
        Log(handle,RDLC_LOG_ERR,"RxBuffer overflow!");
        prvRxBufferReset(handle);
        return RDLC_ERR_NOT_ALLOWED;
    }
    handle->rxBuf[handle->rxIndexer] = data;
    handle->rxIndexer++;
    return RDLC_OK;
}

/**
 *@brief 从完整接收的缓冲区中读取载荷长度字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetPayloadLen(RdlcStaticHandle_t *handle)
{
    // 接收缓冲区内的结构：源地址 目的地址 载荷长度 载荷 CRC16
    uint16_t res = (((uint16_t)handle->rxBuf[3])<<8) | ((uint16_t)handle->rxBuf[2]);
    return res;
}
/**
 *@brief 从完整接收的缓冲区中读取地址字段
 *@addtogroup 接收缓冲区操作
**/
static inline RdlcAddr_t prvRxBufferGetAddr(RdlcStaticHandle_t *handle)
{
    // 接收缓冲区内的结构：源地址 目的地址 载荷长度 载荷 CRC16
    RdlcAddr_t retval;
    retval.srcAddr = handle->rxBuf[0];
    retval.dstAddr = handle->rxBuf[1];
    return retval;
}
/**
 *@brief 从完整接收的缓冲区中读取CRC字段所在的下标
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetCrcIndex(RdlcStaticHandle_t *handle)
{
    // 接收缓冲区内的结构：源地址 目的地址 载荷长度 载荷 CRC16
    uint16_t res = 4 + prvRxBufferGetPayloadLen(handle);
    return res;
}
/**
 *@brief 从完整接收的缓冲区中读取载荷字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint8_t *prvRxBufferGetPayload(RdlcStaticHandle_t *handle)
{
    // 接收缓冲区内的结构：源地址 目的地址 载荷长度 载荷 CRC16
    return &(handle->rxBuf[4]);
}
/**
 *@brief 从完整接收的缓冲区中读取CRC字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetCrcFromFrame(RdlcStaticHandle_t *handle)
{
    uint16_t payloadSize = prvRxBufferGetPayloadLen(handle);
    uint16_t res = (((uint16_t)handle->rxBuf[payloadSize+5])<<8) | ((uint16_t)handle->rxBuf[payloadSize+4]);
}
/**
 *@brief 从完整接收的缓冲区中计算CRC字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetCrcFromCalc(RdlcStaticHandle_t *handle)
{
    uint16_t res = prvGetCrc16(prvRxBufferGetPayload(handle),prvRxBufferGetPayloadLen(handle));
    return res;
}
/**
 *@brief 给定协议参数，获取最小的接收缓冲区的长度
 *@addtogroup 接收缓冲区评估
**/
static inline int prvRxBufferEstimateSize(uint16_t payloadMaxSize)
{
    return 4 + payloadMaxSize + 2;// 地址 + 载荷长度 + 载荷 + CRC
}
/**
 *@brief 给定接收缓冲区的长度，获取最大允许的协议参数
 *@addtogroup 接收缓冲区评估
**/
static inline int prvRxBufferEstimateMaxPayloadSize(uint16_t bufferSize)
{
    return bufferSize - 6;
}
/**
 *@brief 转义写入发送缓冲区(帧中数据)
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedCommon(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t size,uint16_t *iter,uint8_t data)
{
    if (*iter == size) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }

    if (data == BYTE_ESCAPE) {
        buffer[*iter] = BYTE_ESCAPE;
        (*iter)++;
        if (*iter == size) {
            Log(handle,RDLC_LOG_ERR,"TxBuffer overflow!");
            return RDLC_ERR_NOT_ALLOWED;
        }
    }

    buffer[*iter] = data;
    (*iter)++;
    return RDLC_OK;
}
/**
 *@brief 转义写入发送缓冲区(帧头帧尾数据)
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedFrame(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t size,uint16_t *iter,uint8_t frameData)
{
    if (*iter == size) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }

    buffer[*iter] = BYTE_ESCAPE;
    (*iter)++;
    if (*iter == size) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }

    buffer[*iter] = frameData;
    (*iter)++;
    return RDLC_OK;
}

/**
 *@brief 在发送缓冲区中的指定位置写入帧头
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedHead(RdlcStaticHandle_t *handle,RdlcAddr_t addr,uint8_t *buffer,uint16_t size,uint16_t *iter,uint16_t payloadSize,uint8_t ctrlByte)
{
    int err;
    // 包头
    err = prvTxBufferFeedFrame(handle,buffer,size,iter,BYTE_HEAD);
    if (err != RDLC_OK) return err;

    // 源地址
    err = prvTxBufferFeedCommon(handle,buffer,size,iter,addr.srcAddr);
    if (err != RDLC_OK) return err;

    // 目的地址
    err = prvTxBufferFeedCommon(handle,buffer,size,iter,addr.dstAddr);
    if (err != RDLC_OK) return err;

    // 载荷长度
    uint8_t payloadHigh = (payloadSize & 0xFF00) >> 8;
    uint8_t payloadLow  = (payloadSize & 0x00FF);
    err = prvTxBufferFeedCommon(handle,buffer,size,iter,payloadLow);
    if (err != RDLC_OK) return err;
    err = prvTxBufferFeedCommon(handle,buffer,size,iter,payloadHigh);
    if (err != RDLC_OK) return err;

    return RDLC_OK;
}
/**
 *@brief 在发送缓冲区中的指定位置写入载荷
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedPayload(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t bufferSize,uint16_t *iter,uint8_t *payload,uint16_t payloadSize)
{
    // 提前预判是否会超出范围
    uint8_t count = 0;
    for(uint16_t i=0;i<payloadSize;i++)
        if (payload[i] == BYTE_ESCAPE)
            count++;
    if (*iter + count >= bufferSize) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer feed payload overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }

    for(uint16_t i=0;i<payloadSize;i++)
        prvTxBufferFeedCommon(handle,buffer,bufferSize,iter,payload[i]);
    return RDLC_OK;
}
/**
 *@brief 在发送缓冲区中的指定位置写入帧尾
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedTail(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t bufferSize,uint16_t *iter,uint16_t crc16)
{
    //crc
    uint8_t crcHigh = (crc16 & 0xFF00) >> 8;
    uint8_t crcLow  = (crc16 & 0x00FF);
    int err;

    err = prvTxBufferFeedCommon(handle,buffer,bufferSize,iter,crcLow);
    if (err != RDLC_OK) return err;
    err = prvTxBufferFeedCommon(handle,buffer,bufferSize,iter,crcHigh);
    if (err != RDLC_OK) return err;

    // 包尾
    err = prvTxBufferFeedFrame(handle,buffer,bufferSize,iter,BYTE_TAIL);
    if (err != RDLC_OK) return err;
    return RDLC_OK;
}
/**
 *@brief 给定协议参数，获取最小的发送缓冲区的长度
 *@addtogroup 发送缓冲区评估
**/
static inline int prvTxBufferEstimateSize(uint16_t msgMaxSize,uint16_t msgMaxEscapeSize)
{
    // 0xFF 0xC0 0xFF SRC 0xFF DST 0xFF LENL 0xFF LENH
    // 0xFF CRCL 0xFF CRCH 0xFF 0x0C
    return 10 + msgMaxSize + msgMaxEscapeSize + 6;// 最大转义头 + 数据 + 转义 + 最大转义尾
}
/**
 *@brief 给定发送缓冲区的长度，获取最大允许的协议参数
 *@addtogroup 发送缓冲区评估
**/
static inline int prvTxBufferEstimateMessageSize(uint16_t bufferSize)
{
    return bufferSize - 16;
}
/**
 *@brief 转义状态机
 *@return 正数代表转义出的字符，负数代表尚未完成转义或出了问题
 *@addtogroup 状态机
**/
static inline int prvRxFsmEscape(uint8_t *stateEscape,uint8_t byte)
{
    switch(*stateEscape)
    {
        // 如果这个字节是转义字节，就等到下个字节；否则返回这个字节
        case RDLC_STATE_ESCAPE_WAIT:
            if (byte == BYTE_ESCAPE){
                *stateEscape = RDLC_STATE_ESCAPE_GET;
                return RDLC_NOT_FINISH;
            }
            else {
                return byte;
            }
        break;

        // 只有正确转义才会返回字节，否则返回错误码
        case RDLC_STATE_ESCAPE_GET:
            *stateEscape = RDLC_STATE_ESCAPE_WAIT;
            if (byte == BYTE_ESCAPE) {
                return BYTE_ESCAPE;
            }
            else if (byte == BYTE_HEAD) {
                return BYTE_HEAD;
            }
            else if (byte == BYTE_TAIL) {
                return BYTE_TAIL;
            }
            else {
                return RDLC_ERR_NOT_ALLOWED;
            }
        break;
    }
}
/**
 *@brief  带有冗余越界保护的解析状态机
 *@return 1成功-1失败0尚未完成
 *@note   冗余保护：不仅在收到载荷长度时检查越界，也在接收的过程中检查越界
 *@addtogroup 状态机
**/
static inline int prvRxFsmParse(RdlcStaticHandle_t *handle,uint8_t byte)
{
    uint16_t crcFromBuf;
    uint16_t crcFromFrame;

    switch(handle->stateParse)
    {
        // 等待帧头
        case RDLC_STATE_PARSE_WAIT_HEAD:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitHead,read=%#hhX",byte);

            if (byte == BYTE_HEAD)
                handle->stateParse = RDLC_STATE_PARSE_GET_SRCADDR;
        break;

        // 等待源地址
        case RDLC_STATE_PARSE_GET_SRCADDR:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitSrcAddr,read=%#hhX",byte);
            prvRxBufferFeed(handle,byte);
            handle->stateParse = RDLC_STATE_PARSE_GET_DSTADDR;
        break;

        // 等待目标地址
        case RDLC_STATE_PARSE_GET_DSTADDR:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitDstAddr,read=%#hhX",byte);
            prvRxBufferFeed(handle,byte);
            handle->stateParse = RDLC_STATE_PARSE_GET_LENL;
        break;

        // 等待载荷长度低八位
        case RDLC_STATE_PARSE_GET_LENL:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitPayloadLenL,read=%#hhX",byte);
            prvRxBufferFeed(handle,byte);
            handle->stateParse = RDLC_STATE_PARSE_GET_LENH;
        break;

        // 等待载荷长度高八位
        case RDLC_STATE_PARSE_GET_LENH:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitPayloadLenH,read=%#hhX",byte);

            prvRxBufferFeed(handle,byte);
            handle->payloadSize = prvRxBufferGetPayloadLen(handle);

            handle->stateParse = RDLC_STATE_PARSE_GET_PAYLOAD;
        break;

        // 等待载荷
        case RDLC_STATE_PARSE_GET_PAYLOAD:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitPayload,read=%#hhX",byte);

            prvRxBufferFeed(handle,byte);

            if (handle->rxIndexer == prvRxBufferGetCrcIndex(handle))
                handle->stateParse = RDLC_STATE_PARSE_GET_CRCL;
        break;

        // 等待CRC低八位
        case RDLC_STATE_PARSE_GET_CRCL:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitCrcL,read=%#hhX",byte);

            prvRxBufferFeed(handle,byte);

            handle->stateParse = RDLC_STATE_PARSE_GET_CRCH;
        break;

        // 等待CRC高八位
        case RDLC_STATE_PARSE_GET_CRCH:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitCrcH,read=%#hhX",byte);

            prvRxBufferFeed(handle,byte);

            handle->stateParse = RDLC_STATE_PARSE_GET_TAIL;
        break;

        // 检查包尾和CRC是否正确
        case RDLC_STATE_PARSE_GET_TAIL:
            Log(handle,RDLC_LOG_DEBUG,"state=CheckTail,read=%#hhX",byte);

            crcFromBuf = prvRxBufferGetCrcFromCalc(handle);
            crcFromFrame = prvRxBufferGetCrcFromFrame(handle);
            handle->stateParse = RDLC_STATE_PARSE_WAIT_HEAD;

            if ((crcFromBuf == crcFromFrame) && (byte == BYTE_TAIL)) {
                if (handle->cbParsed == NULL)
                    Log(handle,RDLC_LOG_DEBUG,"crc pass but no callback specified");
                else {
                    handle->cbParsed(handle,prvRxBufferGetAddr(handle),prvRxBufferGetPayload(handle),prvRxBufferGetPayloadLen(handle));
                    Log(handle,RDLC_LOG_DEBUG,"crc pass and callback");
                }
                prvRxBufferReset(handle);
                return RDLC_OK;
            }
            else {
                Log(handle,RDLC_LOG_WARN,"crc failed for %#hX vs %#hX",crcFromBuf,crcFromFrame);
                prvRxBufferReset(handle);
                return RDLC_ERR_CRC;
            }
        break;
    }
    return RDLC_NOT_FINISH;
}
/**
 * @brief 创建一个RDLC协议实例
 *
 * @param config RDLC协议的配置，不应为NULL
 * @param port   RDLC所需的硬件对接接口，不应为NULL
 * @return Rdlc_t 成功则返回实例，失败则返回NULL
 */
Rdlc_t xRdlcCreate(const RdlcConfig_t *config, const RdlcPort_t *port)
{
    if (!config || !port || !port->portMalloc || !port->portFree) {
        return NULL;
    }

    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t *)port->portMalloc(sizeof(RdlcStaticHandle_t));
    if (!handle) {
        return NULL;
    }
    memset(handle, 0, sizeof(RdlcStaticHandle_t));

    handle->rxBufSize = prvRxBufferEstimateSize(config->msgMaxSize);
    handle->rxBuf = port->portMalloc(handle->rxBufSize);
    if (!handle->rxBuf) {
        port->portFree(handle);
        return NULL;
    }

    handle->payloadMaxEscapeSize = config->msgMaxEscapeSize;
    handle->payloadMaxSize = config->msgMaxSize;

    handle->cbParsed  = config->cbParsed;
    handle->cbError   = config->cbError;
    memcpy(&handle->port, port, sizeof(RdlcPort_t));
    handle->logLevel = RDLC_LOG_NONE;
    return (Rdlc_t)handle;
}
/**
 * @brief 删除一个RDLC实例
 *
 * @param protoHandle RDLC实例
 */
void vRdlcDestroy(Rdlc_t protoHandle)
{
    if (!protoHandle) return;

    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t *)protoHandle;
    if (handle->rxBuf && handle->port.portFree) {
        handle->port.portFree(handle->rxBuf);
    }
    if (handle->port.portFree) {
        handle->port.portFree(handle);
    }
}
/**
 * @brief 将一个字节送入RDLC实例中进行解析
 *
 * @param protoHandle RDLC实例
 * @param byte 输入的字节
 * @return int 错误状态码
 */
int xRdlcReadByte(Rdlc_t protoHandle, uint8_t byte)
{
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    int status = RDLC_NOT_FINISH;
    if (!protoHandle) {
        Log(handle,RDLC_LOG_ERR,"invalid arguments for xRdlcReadByte");
        return RDLC_ERR_INVALID_ARG;
    }
    // 仅把转义后的字符送入状态机
    int realByte = prvRxFsmEscape(&(handle->stateEscape),byte);
    if (realByte >= RDLC_OK){
        status = prvRxFsmParse(handle,realByte);
    }
    return status;
}
/**
 * @brief 将多个字节送入RDLC实例中进行解析
 *
 * @param protoHandle RDLC实例
 * @param buffer 输入的字节数组
 * @param size 数组的长度
 * @return int 错误状态码
 */
int xRdlcReadBytes(Rdlc_t protoHandle, uint8_t *buffer, uint16_t size)
{
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    int res = RDLC_NOT_FINISH;
    if (!protoHandle || !buffer) {
            Log(handle,RDLC_LOG_ERR,"invalid arguments for xRdlcReadBytes");
            return RDLC_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < size; ++i) {
        res = xRdlcReadByte(protoHandle, buffer[i]);
        if (res != RDLC_OK && res != RDLC_NOT_FINISH) return res;
    }
    return res;
}
/**
 * @brief 对原始数据进行转义和封包
 *
 * @param protoHandle RDLC实例
 * @param addr 目的地址和源地址
 * @param payload 原始数据所在地址
 * @param payloadSize 原始数据长度
 * @param frameBuf 封包后的数据要放在什么位置
 * @param frameMaxSize 允许封包后的最大长度
 * @return int 封包后的RDLC数据包长度
 */
int xRdlcWriteBytes(Rdlc_t protoHandle,RdlcAddr_t addr,
                    const uint8_t *payload,uint16_t payloadSize,
                    uint8_t *frameBuf,uint16_t frameMaxSize)
{
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    int err;

    if (!protoHandle || !payload || !frameBuf) {
        Log(handle,RDLC_LOG_ERR,"invalid arguments for xRdlcWriteBytes");
        return RDLC_ERR_INVALID_ARG;
    }
    if ((frameMaxSize < prvTxBufferEstimateSize(handle->payloadMaxSize,handle->payloadMaxEscapeSize))||
        (payloadSize > handle->payloadMaxSize)) {
        Log(handle,RDLC_LOG_ERR,"frame buffer too short.expect %hd but %hd",prvTxBufferEstimateSize(handle->payloadMaxSize,handle->payloadMaxEscapeSize),frameMaxSize);
        return RDLC_ERR_BUFFER_TOO_SHORT;
    }

    uint16_t itr = 0;
    uint16_t crc16 = prvGetCrc16(payload,payloadSize);

    err = prvTxBufferFeedHead(handle,addr,frameBuf,frameMaxSize,&itr,payloadSize,0x0);
    if (err != RDLC_OK) return err;

    err = prvTxBufferFeedPayload(handle,frameBuf,frameMaxSize,&itr,payload,payloadSize);
    if (err != RDLC_OK) return err;

    err = prvTxBufferFeedTail(handle,frameBuf,frameMaxSize,&itr,crc16);
    if (err != RDLC_OK) return err;


    return itr;
}
/**
 * @brief 复位RDLC实例的接收状态
 *
 * @param protoHandle RDLC实例
 * @return int 错误状态码
 */
void vRdlcReset(Rdlc_t protoHandle)
{
    if (!protoHandle) return -1;
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;

    memset(handle->rxBuf,0,handle->rxBufSize);
    handle->payloadSize = 0;
    handle->rxIndexer = 0;
    handle->stateEscape = 0;
    handle->stateParse = RDLC_STATE_PARSE_WAIT_HEAD;
    handle->stateEscape = RDLC_STATE_ESCAPE_WAIT;

    return 0;
}
/**
 * @brief 获取RDLC实例的内部解析状态
 *
 * @param protoHandle RDLC实例
 * @return int 解析状态机状态
 */
int xRdlcGetParseState(Rdlc_t protoHandle)
{
    if (!protoHandle) return -1;
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    return handle->stateParse;
    return 0;
}
/**
 * @brief 获取RDLC实例的内部转义状态
 *
 * @param protoHandle RDLC实例
 * @return int 解析状态机状态
 */
int xRdlcGetEscapeState(Rdlc_t protoHandle)
{
    if (!protoHandle) return -1;
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    return handle->stateEscape;
    return 0;
}
/**
 * @brief 获取RDLC实例的日志层次
 *
 * @param protoHandle RDLC实例
 * @return RdlcLogLevel_t 日志详细程度
 */
RdlcLogLevel_t xRdlcGetLogLevel(Rdlc_t protoHandle)
{
    if (!protoHandle) return RDLC_LOG_NONE;
    return ((RdlcStaticHandle_t *)protoHandle)->logLevel;
}
/**
 * @brief 设置RDLC实例的日志层次
 *
 * @param protoHandle RDLC实例
 * @param level 日志详细程度
 */
void vRdlcSetLogLevel(Rdlc_t protoHandle,RdlcLogLevel_t level)
{
    if (!protoHandle) return -1;
    RdlcStaticHandle_t *handle = (RdlcStaticHandle_t*)protoHandle;
    handle->logLevel = level;
}
