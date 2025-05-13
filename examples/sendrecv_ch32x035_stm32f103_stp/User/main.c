/************************************************************************
 * 
 * @brief RDLC协议在CH32x035上运行的例程，改编自官方USART_Polling例程
 * 
 * @note  CH32的标准库代码和STM32的标准库代码几乎一模一样，亦可参考本例程
 * 
 * @note 本例程通过USART2(PA2-PA3)发送数据，USART3(PB3-PB4)收数据
 * 
***********************************************************************/

#include "debug.h"
#inclde "../../../rdlc.h"

/* Global typedef */
typedef enum
{
    FAILED = 0,
    PASSED = !FAILED
} TestStatus;

/* Global define */
#define TxSize     (size(TxBuffer))
#define size(a)    (sizeof(a) / sizeof(*(a)))

/* Golbal Consts */
const RdlcConfig_t g_protoConfig = {  //RDLC协议配置
    .msgMaxSize = 16,
    .msgMaxEscapeSize = 16,
    .cbError = NULL,
    .cbParsed = NULL,
};
const RdlcPort_t g_protoPort = { // RDLC系统调用
    .portFree = free,
    .portMalloc = malloc,
    .portPrintf = NULL,
};

/* Global Variable */
u8         TxBuffer[sizeof(TxBuffer)*2]; // 需要根据情况自行决定最大帧长度，不一定是*2
u8         RxBuffer[TxSize] = {0};
u8         TxCnt = 0, RxCnt = 0;
TestStatus TransferStatus = FAILED;

Rdlc_t     g_rdlcProto;
u8         g_txMsg[] = "Hello!I am message parsed from RDLC!\n";


/*********************************************************************
 * @fn      Buffercmp
 *
 * @brief   Compares two buffers
 *
 * @param   Buf1,Buf2 - buffers to be compared
 *          BufferLength - buffer's length
 *
 * @return  PASSED - Buf1 identical to Buf
 *          FAILED - Buf1 differs from Buf2
 */
TestStatus Buffercmp(uint8_t *Buf1, uint8_t *Buf2, uint16_t BufLength)
{
    while(BufLength--)
    {
        if(*Buf1 != *Buf2)
        {
            return FAILED;
        }
        Buf1++;
        Buf2++;
    }
    return PASSED;
}

/*********************************************************************
 * @fn      USARTx_CFG
 *
 * @brief   Initializes the USART2 & USART3 peripheral.
 *
 * @return  none
 */
void USARTx_CFG(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* USART2 TX-->A.2   RX-->A.3 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    /* USART3 TX-->B.3  RX-->B.4 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);

    USART_Init(USART3, &USART_InitStructure);
    USART_Cmd(USART3, ENABLE);
}

/*********************************************************************
 * @fn      RDLC_CFG
 *
 * @brief   初始化RDLC协议
 *
 * @return  none
 */
void RDLC_CFG(void)
{
    g_rdlcProto = xRdlcCreate(&g_protoConfig,&g_protoPort);
    if (g_rdlcProto == NULL)
        printf("RDLC: No Memory!\n");
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    printf("USART-RDLC TEST\r\n");
    USARTx_CFG(); /* USART2 & USART3 INIT */
    RDLC_CFG();   /* RDLC Protocol INIT */

    while(TxCnt < TxSize)
    {
        USART_SendData(USART2, TxBuffer[TxCnt++]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
        {
            /* waiting for sending finish */
        }
        while(USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == RESET)
        {
            /* waiting for receiving finish */
        }
        RxBuffer[RxCnt++] = (USART_ReceiveData(USART3));
    }

    TransferStatus = Buffercmp(TxBuffer, RxBuffer, TxSize);

    if(TransferStatus)
    {
        printf("send success!\r\n");
        printf("TXBuffer: %s \r\n", TxBuffer);
        printf("RxBuffer: %s \r\n", RxBuffer);
    }
    else
    {
        printf("send fail!\r\n");
        printf("TXBuffer: %s \r\n", TxBuffer);
        printf("RxBuffer: %s \r\n", RxBuffer);
    }
    while(1)
    {
    }
}

/********************************** (C) 原始版权信息 *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/26
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
