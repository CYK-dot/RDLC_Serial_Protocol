#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/// ���ú�
#define RDLC_CRC16_USE_CALCULATE  0
#define RDLC_CRC16_USE_TABLE      1

/// ��־���
typedef enum{
    RDLC_LOG_DEBUG = 0,
    RDLC_LOG_INFO  = 1,
    RDLC_LOG_WARN  = 2,
    RDLC_LOG_ERR   = 3,
    RDLC_LOG_NONE  = 4,
}RdlcLogLevel_t;

/// ������
#define RDLC_OK 0
#define RDLC_NOT_FINISH -1
#define RDLC_ERR_NOT_ALLOWED -2
#define RDLC_ERR_CRC -3
#define RDLC_ERR_INVALID_ARG -4
#define RDLC_ERR_BUFFER_TOO_SHORT -5

// ת��״̬
#define RDLC_STATE_ESCAPE_WAIT 0 ///< ����ת��
#define RDLC_STATE_ESCAPE_GET  1 ///< �ȴ�ת��
// ����״̬
#define RDLC_STATE_PARSE_WAIT_HEAD 0   ///< �ȴ�֡ͷ
#define RDLC_STATE_PARSE_GET_CTRL 1    ///< �ȴ������ֶ�
#define RDLC_STATE_PARSE_GET_LENL 2    ///< �ȴ��غɳ��ȵͰ�λ
#define RDLC_STATE_PARSE_GET_LENH 3    ///< �ȴ��غɳ��ȸ߰�λ
#define RDLC_STATE_PARSE_GET_PAYLOAD 4 ///< �ȴ��غ�
#define RDLC_STATE_PARSE_GET_CRCL 5    ///< �ȴ�У����Ͱ�λ
#define RDLC_STATE_PARSE_GET_CRCH 6    ///< �ȴ�У����߰�λ
#define RDLC_STATE_PARSE_GET_TAIL 7    ///< �ȴ�֡β

// �ײ�ӿڶ���
typedef void* (*RdlcMalloc_fptr)(size_t);
typedef void  (*RdlcFree_fptr)  (void*);
typedef int   (*RdlcPrintf_fptr)(RdlcLogLevel_t level,const char *fmt,va_list args);

// �����ӿ����Ͷ���
typedef void* Rdlc_t;
typedef int (*RdlcOnParse_fptr) (Rdlc_t,const uint8_t*,uint16_t);
typedef int (*RdlcOnError_fptr) (Rdlc_t,int);

/// ��������
typedef struct{
    uint16_t msgMaxSize;
    uint16_t msgMaxEscapeSize;
    RdlcOnParse_fptr cbParsed;
    RdlcOnError_fptr cbError;
}RdlcConfig_t;

/// �ӿ�����
typedef struct{
    RdlcMalloc_fptr portMalloc;
    RdlcFree_fptr portFree;
    RdlcPrintf_fptr portPrintf;
}RdlcPort_t;

// ���캯������������
Rdlc_t xRdlcCreate(const RdlcConfig_t *config,const RdlcPort_t *port);
void   vRdlcDestroy(Rdlc_t protoHandle);

// ���󷽷�1�����
int xRdlcReadByte(Rdlc_t protoHandle,uint8_t byte);
int xRdlcReadBytes(Rdlc_t protoHandle,uint8_t *buffer,uint16_t size);

// ���󷽷�2�����
int xRdlcWriteBytes(Rdlc_t protoHandle,const uint8_t *payload,uint16_t payloadSize,
                    uint8_t *frameBuf,uint16_t frameMaxSize);

// ���󷽷�3������
int xRdlcReset(Rdlc_t protoHandle);

// ���󷽷�4������״̬
int xRdlcGetParseState(Rdlc_t protoHandle);
int xRdlcGetEscapeState(Rdlc_t protoHandle);

// �����Ա1����־���
RdlcLogLevel_t xRdlcGetLogLevel(Rdlc_t protoHandle);
void vRdlcSetLogLevel(Rdlc_t protoHandle,RdlcLogLevel_t level);


#ifdef __cplusplus
}
#endif
