#include "project.h"
#include "arm_math.h"
#include "stdlib.h"

#ifdef USE_FIR
#include "FIRCoeffs.h"
#else
#include "IIRCoeffs.h"    
#endif

volatile int16_t  buffer1[256];
volatile int16_t  buffer2[256];
volatile int16_t  buffer3[256];
volatile int16_t * buffers[3] = {buffer1, buffer2, buffer3};

volatile uint8_t flag = 0;

void ADC_BuffFullInt() {
    Cy_DMA_Channel_ClearInterrupt(ADCDMA_HW, ADCDMA_DW_CHANNEL);
    NVIC_ClearPendingIRQ(ADCBuffInt_cfg.intrSrc);
    ++flag;
}
void hardware_config() {
    Opamp_Start();
    ADC_Start();
    ADC_StartConvert();
    DAC_Start();
    
    ADCDMA_Init();
    DACDMA_Init();
    
    Cy_DMA_Descriptor_SetSrcAddress(&ADCDMA_ADCDMA_Desc1, (void*)&SAR->CHAN_RESULT[0]);
    Cy_DMA_Descriptor_SetSrcAddress(&ADCDMA_ADCDMA_Desc2, (void*)&SAR->CHAN_RESULT[0]);
    Cy_DMA_Descriptor_SetSrcAddress(&ADCDMA_ADCDMA_Desc3, (void*)&SAR->CHAN_RESULT[0]);
    Cy_DMA_Descriptor_SetDstAddress(&ADCDMA_ADCDMA_Desc1, (void*)buffer1);
    Cy_DMA_Descriptor_SetDstAddress(&ADCDMA_ADCDMA_Desc2, (void*)buffer2);
    Cy_DMA_Descriptor_SetDstAddress(&ADCDMA_ADCDMA_Desc3, (void*)buffer3);
    
    Cy_DMA_Descriptor_SetSrcAddress(&DACDMA_DACDMA_Desc1, (void*)buffer2);
    Cy_DMA_Descriptor_SetSrcAddress(&DACDMA_DACDMA_Desc2, (void*)buffer3);
    Cy_DMA_Descriptor_SetSrcAddress(&DACDMA_DACDMA_Desc3, (void*)buffer1); 
    Cy_DMA_Descriptor_SetDstAddress(&DACDMA_DACDMA_Desc1, (void*)&DAC_CTDAC_HW->CTDAC_VAL_NXT);
    Cy_DMA_Descriptor_SetDstAddress(&DACDMA_DACDMA_Desc2, (void*)&DAC_CTDAC_HW->CTDAC_VAL_NXT);
    Cy_DMA_Descriptor_SetDstAddress(&DACDMA_DACDMA_Desc3, (void*)&DAC_CTDAC_HW->CTDAC_VAL_NXT);
  
    Cy_DMA_Channel_SetInterruptMask(ADCDMA_HW, ADCDMA_DW_CHANNEL, CY_DMA_INTR_MASK);
    Cy_SysInt_Init(&ADCBuffInt_cfg, ADC_BuffFullInt);
    NVIC_EnableIRQ(ADCBuffInt_cfg.intrSrc);
    
    Cy_DMA_Channel_Enable(ADCDMA_HW, ADCDMA_DW_CHANNEL);
    Cy_DMA_Channel_Enable(DACDMA_HW, DACDMA_DW_CHANNEL);
}


#ifdef USE_FIR
void run_filter() {
    arm_fir_instance_q15 firInstance;
    q15_t pState[BL+256];
    arm_fir_init_q15(&firInstance, BL, B, pState, 256);
    hardware_config();
    
    while(1) {
        for (int i = 0; i < 3; i++) {
            while(!flag);
            flag = 0;
            arm_fir_q15(&firInstance, buffers[i], buffers[i], 256);
        }
    }
}
#else

q15_t * convertCoeffs() {
    q15_t * coeffs = (q15_t *)malloc(6*MWSPT_NSEC*sizeof(q15_t));
    
    for (int i = 0; i < MWSPT_NSEC; i++) {
        coeffs[i*6] = NUM[i][0];
        coeffs[i*6+1] = 0;
        coeffs[i*6+2] = NUM[i][1];
        coeffs[i*6+3] = NUM[i][2];
        coeffs[i*6+4] = -DEN[i][1];
        coeffs[i*6+5] = -DEN[i][2];
    }
    return coeffs;
}


void run_filter() {
    arm_biquad_casd_df1_inst_q15 iirInstance;
    q15_t pState[4*MWSPT_NSEC];
    q15_t * coeffs = convertCoeffs();
    q31_t * input;
    q31_t * dest;
    arm_biquad_cascade_df1_init_q15(&iirInstance, MWSPT_NSEC, coeffs, pState, 1);
    
    hardware_config();
    while(1) {
        for (int i = 0; i < 3; i++) {
            while(!flag);
            flag = 0;
            dest = (q31_t*)buffers[i];
            input = (q31_t*)buffers[i];
            for (int j = 0; j < 16; j++) {
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
               *(*&input) = __SADD16(*input++, 0x04000400);
            }
            
            arm_biquad_cascade_df1_q15(&iirInstance, buffers[i],buffers[i], 256);
        }
    }
}
#endif
    
int main(void)
{
    __enable_irq();
    run_filter();

}

/* [] END OF FILE */
