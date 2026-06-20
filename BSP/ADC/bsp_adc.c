#include "bsp_adc.h"

#define DMA_ADC_CPLT_INT 0xA1

uint32_t * buffer1 = NULL;
uint32_t * buffer2 = NULL;

void DMA_Configuration(void) {
    // 使能DMA2时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // 配置DMA2通道0
    DMA_InitTypeDef DMA_InitStructure;
    DMA_DeInit(DMA2_Stream0);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    // DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)buffer1;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    // DMA_InitStructure.DMA_BufferSize = BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    // DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;  // 循环模式
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;  // 循环模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);

    /* 启用DMA传输完成中断 */
    DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);

    /* 使能DMA2 Stream0 */
    // DMA_Cmd(DMA2_Stream0, ENABLE);
}

int8_t adc_buffer_init(void) {
    buffer1 = (uint32_t *)malloc(BUFFER_SIZE * sizeof(uint32_t));
    buffer2 = (uint32_t *)malloc(BUFFER_SIZE * sizeof(uint32_t));

    if (buffer1 == NULL || buffer2 == NULL) {
        log_e("buffer malloc failed");
        return -1;
    }

    memset(buffer1, 0xff, BUFFER_SIZE * sizeof(uint32_t));
    memset(buffer2, 0xff, BUFFER_SIZE * sizeof(uint32_t));

    return 0;
}

void bsp_adc_init(void) {
 //    if (adc_buffer_init() != 0) return;
 //
 //    if (adc_quque_init() != 0) {
	// 	log_e("adc_queue_init error");
	// 	return;
	// }
 //
 //    if (adc_quque2_init() != 0) {
	// 	log_e("adc_queue2_init error");
	// 	return;
	// }

    log_i("adc init start");

    // 使能GPIOA时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    // 使能ADC1时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* 配置PA0为模拟输入 (对应ADC通道0) */
    GPIO_InitTypeDef GPIO_ADC1_InitStructure;
    GPIO_ADC1_InitStructure.GPIO_Pin = ADC_GPIO_PIN;
    GPIO_ADC1_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_ADC1_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(ADC_GPIO_PORT, &GPIO_ADC1_InitStructure);

    // ADC通用配置
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&ADC_CommonInitStructure);

    // ADC1配置
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;   // 软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 配置ADC1通道0采样时间
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_3Cycles);

    NVIC_InitTypeDef NVIC_InitStructure;
    /* 配置DMA2通道0 */
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    log_i("dma init start");
    DMA_Configuration();


    /* 启用ADC1的DMA请求 */
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);

    log_i("dma init end");
    // 使能ADC1
    ADC_Cmd(ADC1, ENABLE);

    // 启用ADC1的软件触发转换模式
    // ADC_SoftwareStartConv(ADC1);

    log_i("adc and dma init ok");
}

// QueueHandle_t adc_queue;
// int adc_quque_init(void) {
//     adc_queue = xQueueCreate(BUFFER_SIZE, sizeof(uint32_t));
//     if (adc_queue == NULL) {
//         log_e("adc queue malloc failed");
//         return -1;
//     }
//     return 0;
// }
//
// QueueHandle_t adc_queue2;
// int adc_quque2_init(void) {
//     adc_queue2 = xQueueCreate(BUFFER_SIZE, sizeof(uint32_t));
//     if (adc_queue2 == NULL) {
//         log_e("adc queue2 malloc failed");
//         return -1;
//     }
//     return 0;
// }



void bsp_adc_change_dma_mem_addr(uint32_t* addr) {
    // 先停止DMA
    DMA_Cmd(DMA2_Stream0, DISABLE);

    // 等待DMA停止
    while (DMA_GetCmdStatus(DMA2_Stream0) != DISABLE) {};

    // 修改DMA内存地址
    DMA2_Stream0->M0AR = (uint32_t)addr;

    // 启动DMA
    DMA_Cmd(DMA2_Stream0, ENABLE);

    // uint32_t dummy = 0;
    // xQueueReceive(adc_queue, &dummy, 0);
}

/**
 * @brief  重新启动 ADC DMA 传输
 * @param  ADCx: ADC 外设指针 (e.g., ADC1)
 * @param  DMA_Streamx: DMA 流指针 (e.g., DMA2_Stream0)
 * @param  pBuffer: 目标内存缓冲区地址
 * @param  Length: 要传输的数据项数量
 * @retval None
 */
void ADC_DMA_Restart(ADC_TypeDef* ADCx, DMA_Stream_TypeDef* DMA_Streamx, uint32_t* pBuffer, uint32_t Length) {
    // 1. 确保 DMA Stream 已经停止
    DMA_Cmd(DMA_Streamx, DISABLE);
    // 等待 DMA Stream 完全停止 (检查 EN 位)
    while (DMA_GetCmdStatus(DMA_Streamx) != DISABLE) {}

    // 2. 更新 DMA 目标内存地址和传输数量
    // 注意：必须在 DMA 禁用状态下才能安全地修改这些寄存器
    DMA_Streamx->M0AR = (uint32_t)pBuffer; // 直接写入 M0AR 寄存器，设置内存目标地址
    DMA_SetCurrDataCounter(DMA_Streamx, Length); // 设置剩余数据量

    // 3. 清除 DMA 传输完成等可能的标志位 (可选，但推荐)
    // 通常在中断服务程序中清除，但如果担心上次中断未处理完，可以在这里清
    // DMA_ClearFlag(DMA_Streamx, DMA_FLAG_TCIFx); // x 为具体 Stream 号，如 TCIF0 for Stream0

    // 4. 启动 DMA Stream
    DMA_Cmd(DMA_Streamx, ENABLE);

    // 5. 启动 ADC 转换 (如果使用软件触发)
    // 如果使用外部触发，这一步不是必需的
    ADC_ClearFlag(ADCx, ADC_FLAG_EOC | ADC_FLAG_OVR); // 清除上次可能的标志
    ADC_SoftwareStartConv(ADCx); // 开始软件触发转换

    // DMA 会自动将 ADC_DR 的数据搬运到 pBuffer，直到达到 Length 数量
}




