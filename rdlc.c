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

/**
 *@brief RDLC对象的private成员
**/
typedef struct {
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
} RdlcStaticHandle_t;

/**
 *@brief 日志调用
 *@addtogroup 支撑功能
**/
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
/**
 *@brief CRC16(0xA001)计算
 *@addtogroup 支撑功能
**/
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
    // 接收缓冲区内的结构：载荷长度 载荷 CRC16
    uint16_t res = (((uint16_t)handle->rxBuf[1])<<8) | ((uint16_t)handle->rxBuf[0]);
    return res;
}
/**
 *@brief 从完整接收的缓冲区中读取载荷长度字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetCrcIndex(RdlcStaticHandle_t *handle)
{
    // 接收缓冲区内的结构：载荷长度 载荷 CRC16
    uint16_t res = 2 + prvRxBufferGetPayloadLen(handle);
    return res;
}
/**
 *@brief 从完整接收的缓冲区中读取载荷字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint8_t *prvRxBufferGetPayload(RdlcStaticHandle_t *handle)
{
    return &(handle->rxBuf[2]);
}
/**
 *@brief 从完整接收的缓冲区中读取CRC字段
 *@addtogroup 接收缓冲区操作
**/
static inline uint16_t prvRxBufferGetCrcFromFrame(RdlcStaticHandle_t *handle)
{
    uint16_t payloadSize = prvRxBufferGetPayloadLen(handle);
    uint16_t res = (((uint16_t)handle->rxBuf[payloadSize+3])<<8) | ((uint16_t)handle->rxBuf[payloadSize+2]);
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
    return 2 + payloadMaxSize + 2;// 载荷长度 + 载荷 + CRC
}
/**
 *@brief 给定接收缓冲区的长度，获取最大允许的协议参数
 *@addtogroup 接收缓冲区评估
**/
static inline int prvRxBufferEstimateMaxPayloadSize(uint16_t bufferSize)
{
    return bufferSize - 4;
}
/**
 *@brief 转义写入发送缓冲区
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeed(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t size,uint16_t *iter,uint8_t data)
{
    if (*iter == size) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }
    if (data == BYTE_ESCAPE || data == BYTE_HEAD || data == BYTE_TAIL) {
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
 *@brief 在发送缓冲区中的指定位置写入帧头
 *@addtogroup 发送缓冲区操作
**/
static inline int prvTxBufferFeedHead(RdlcStaticHandle_t *handle,uint8_t *buffer,uint16_t size,uint16_t *iter,uint16_t payloadSize,uint8_t ctrlByte)
{
    int err;
    // 包头
    err = prvTxBufferFeed(handle,buffer,size,iter,BYTE_HEAD);
    if (err != RDLC_OK) return err;

    // 控制字节
    err = prvTxBufferFeed(handle,buffer,size,iter,ctrlByte);
    if (err != RDLC_OK) return err;

    // 载荷长度
    uint8_t payloadHigh = (payloadSize & 0xFF00) >> 8;
    uint8_t payloadLow  = (payloadSize & 0x00FF);
    err = prvTxBufferFeed(handle,buffer,size,iter,payloadLow);
    if (err != RDLC_OK) return err;
    err = prvTxBufferFeed(handle,buffer,size,iter,payloadHigh);
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
        if (payload[i] == BYTE_ESCAPE || payload[i] == BYTE_HEAD || payload[i] == BYTE_TAIL)
            count++;
    if (*iter + count >= bufferSize) {
        Log(handle,RDLC_LOG_ERR,"TxBuffer feed payload overflow!");
        return RDLC_ERR_NOT_ALLOWED;
    }

    for(uint16_t i=0;i<payloadSize;i++)
        prvTxBufferFeed(handle,buffer,bufferSize,iter,payload[i]);
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

    err = prvTxBufferFeed(handle,buffer,bufferSize,iter,crcLow);
    if (err != RDLC_OK) return err;
    err = prvTxBufferFeed(handle,buffer,bufferSize,iter,crcHigh);
    if (err != RDLC_OK) return err;

    // 包尾
    err = prvTxBufferFeed(handle,buffer,bufferSize,iter,BYTE_TAIL);
    if (err != RDLC_OK) return err;
    return RDLC_OK;
}
/**
 *@brief 给定协议参数，获取最小的发送缓冲区的长度
 *@addtogroup 发送缓冲区评估
**/
static inline int prvTxBufferEstimateSize(uint16_t msgMaxSize,uint16_t msgMaxEscapeSize)
{
    // 0xFF 0xC0 CTRL 0xFF LENL 0xFF LENH  0xFF CRCL 0xFF CRCH 0xFF 0x0C
    return 7 + msgMaxSize + msgMaxEscapeSize + 6;// 最大转义头 + 数据 + 转义 + 最大转义尾
}
/**
 *@brief 给定发送缓冲区的长度，获取最大允许的协议参数
 *@addtogroup 发送缓冲区评估
**/
static inline int prvTxBufferEstimateMessageSize(uint16_t bufferSize)
{
    return bufferSize - 13;
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
                handle->stateParse = RDLC_STATE_PARSE_GET_CTRL;
        break;

        // 等待控制字节（暂未使用）
        case RDLC_STATE_PARSE_GET_CTRL:
            Log(handle,RDLC_LOG_DEBUG,"state=WaitCtrl,read=%#hhX",byte);

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
                    handle->cbParsed(handle,prvRxBufferGetPayload(handle),prvRxBufferGetPayloadLen(handle));
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
 * @param payload 原始数据所在地址
 * @param payloadSize 原始数据长度
 * @param frameBuf 封包后的数据要放在什么位置
 * @param frameMaxSize 允许封包后的最大长度
 * @return int 封包后的RDLC数据包长度
 */
int xRdlcWriteBytes(Rdlc_t protoHandle, const uint8_t *payload, uint16_t payloadSize,
                    uint8_t *frameBuf, uint16_t frameMaxSize)
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

    err = prvTxBufferFeedHead(handle,frameBuf,frameMaxSize,&itr,payloadSize,0x0);
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
