/**********************************************************************************************************************
 * \file main.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "cy_dmac.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cycfg_dmas.h"
#include "cy_retarget_io.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define BUFFER_SIZE  (36ul)
#define DMAC_SW_TRIG (TRIG_OUT_MUX_3_MDMA_TR_IN0)
#define DMAC_INTR (cpuss_interrupts_dmac_0_IRQn)
#define GPIO_INTERRUPT_PRIORITY (7u)
#define DELAY_MS (1)

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* A flag when DMA transfer is completed, then it will change to true */
static bool g_isComplete;
/* A flag when button interrupt is occurred, then it will change to true */
static bool g_isInterrupt;

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
/* DMAC Interrupt configuration structure */
const cy_stc_sysint_t IRQ_CFG =
{
    .intrSrc = ((NvicMux4_IRQn << 16) | DMAC_INTR),
    .intrPriority = 0UL
};

/* Data to be transferred */
const static  uint8_t    SRC_BUFFER[BUFFER_SIZE] = {0x00,0x01,0x02,0x03,0x04,0x05,
                                                    0x06,0x07,0x08,0x09,0x0A,0x0B,
                                                    0x00,0x01,0x02,0x03,0x04,0x05,
                                                    0x06,0x07,0x08,0x09,0x0A,0x0B,
                                                    0x00,0x01,0x02,0x03,0x04,0x05,
                                                    0x06,0x07,0x08,0x09,0x0A,0x0B};

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* M-DMA Handler */
void HandleDMACIntr(void);

/* GPIO Handler */
static void HandleGPIOIntr(void *handlerArg, cyhal_gpio_event_t event);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/**********************************************************************************************************************
 * Function Name: HandleDMACIntr*
 * Summary:
 *  DMA interrupt handler
 * Parameters:
 *  None
 * Return:
 *  None
 **********************************************************************************************************************
 */
void HandleDMACIntr(void)
{
    uint32_t masked;

    masked = Cy_DMAC_Channel_GetInterruptStatusMasked(DMAC, 0UL);
    if ((masked & CY_DMAC_INTR_COMPLETION) != 0UL)
    {
        /* Clear Complete DMA interrupt flag */
        Cy_DMAC_Channel_ClearInterrupt(DMAC, 0UL,CY_DMAC_INTR_COMPLETION);

        /* Mark the transmission as complete */
        g_isComplete = true;
    }
    else
    {
        CY_ASSERT(false);
    }
}

/**********************************************************************************************************************
 * Function Name: HandleGPIOIntr
 * Summary:
 *   GPIO interrupt handler.
 * Parameters:
 *  void *handlerArg (unused)
 *  cyhal_gpio_event_t (unused)
 **********************************************************************************************************************
 */
static void HandleGPIOIntr(void *handlerArg, cyhal_gpio_event_t event)
{
    g_isInterrupt = true;
}

/**********************************************************************************************************************
 * Function Name: main
 * Summary:
 *  This is the main function for CPU. It...
 *    1.Transmit data from memory array to memory array via M-DMA
 *    2.Using button interrupt to trigger the M-DMA transfer
 *    3.The result will be like below
 *       au8DestBuffer: 0x00,0x01,0x02,0x03,0x04,0x05......
 *       SRC_BUFFER   : 0x00,0x01,0x02,0x03,0x04,0x05......
 * Parameters:
 *  none
 * Return:
 *  int
 **********************************************************************************************************************
 */
int main(void)
{
    cyhal_gpio_callback_data_t gpioBtnCallbackData;
    uint8_t au8DestBuffer[BUFFER_SIZE];

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdtObj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    if (cyhal_wdt_init(&wdtObj, cyhal_wdt_get_max_timeout_ms()) != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(false);
    }
    cyhal_wdt_free(&wdtObj);
#endif /* #if defined (CY_DEVICE_SECURE) */

    /* Initialize the device and board peripherals */
    if (cybsp_init() != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(false);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Initialize the user button */
    cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, CYBSP_BTN_OFF);

    /* Configure GPIO interrupt */
    gpioBtnCallbackData.callback = HandleGPIOIntr;
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &gpioBtnCallbackData);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, GPIO_INTERRUPT_PRIORITY, true);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("**************** M-DMA- memory copy Transfer ****************\r\n");
    printf("\r\n");
    printf("- M-DMA memory copy Transfer Initialize & Enable  \r\n");

    /* Disable M-DMA */
    Cy_DMAC_Disable(DMAC);
    Cy_DMAC_Channel_DeInit(DMAC, 0UL);
 
    /* Set the Source and Destination address */
    /* Source data are located in Code Flash */
    MDMA_Descriptor_0_config.srcAddress = (void *)SRC_BUFFER;
    MDMA_Descriptor_0_config.dstAddress = (void *)au8DestBuffer;

    /* Initialize the M-DMA descriptor */
    if (Cy_DMAC_Descriptor_Init(&MDMA_Descriptor_0, &MDMA_Descriptor_0_config) != CY_DMAC_SUCCESS)
    {
        CY_ASSERT(false);
    }

    /* Initialize the M-DMA channel */
    if (Cy_DMAC_Channel_Init(DMAC, 0UL, &MDMA_channelConfig) != CY_DMAC_SUCCESS)
    {
        CY_ASSERT(false);
    }
    Cy_DMAC_Channel_SetPriority(DMAC, 0UL, 0UL);
    Cy_DMAC_Channel_SetInterruptMask(DMAC, 0UL, CY_DMAC_INTR_COMPLETION);

    /* Enable the M-DMA */
    Cy_DMAC_Enable(DMAC);

    /* Interrupt Initialization */
    if (Cy_SysInt_Init(&IRQ_CFG, HandleDMACIntr) != CY_SYSINT_SUCCESS)
    {
        CY_ASSERT(false);
    }
    /* Enable the Interrupt */
    NVIC_EnableIRQ(NvicMux4_IRQn);

    printf("- M-DMA- memory copy Transfer setup is completed. \r\n");
    printf("\r\n");
    printf("**************** Please Press USER_BTN1. ****************\r\n");
    printf("\r\n");

    for (;;)
    {
        /* Clear the interrupt flag */
        g_isInterrupt = false;

        /* Wait for interrupt */
        while (g_isInterrupt == false)
        {
            cyhal_system_delay_ms(DELAY_MS);
        }

        /* Clear destination memory */
        memset(au8DestBuffer, 0, sizeof(au8DestBuffer));

        /* Set up the channel */
        Cy_DMAC_Channel_SetDescriptor(DMAC, 0UL, &MDMA_Descriptor_0);
        Cy_DMAC_Channel_Enable(DMAC, 0UL);

        /* Clear the transfer completion flag */
        g_isComplete = false;

        /* SW Trigger start */
        if (Cy_TrigMux_SwTrigger(DMAC_SW_TRIG, CY_TRIGGER_TWO_CYCLES) != CY_TRIGMUX_SUCCESS)
        {
            CY_ASSERT(false);
        }

        /* wait for DMA completion */
        while (g_isComplete == false)
        {
            cyhal_system_delay_ms(DELAY_MS);
        }

        printf("- DMA transfer is completed. \r\n");
        printf("\r\n");

        /* Compare source and destination buffers */
        if (memcmp(SRC_BUFFER, au8DestBuffer, BUFFER_SIZE) != 0)
        {
            CY_ASSERT(false);
        }
        
        /* Print out the result of memory copy*/
        printf("**************** Source(CODE FLASH): ****************\r\n");
        for (int idx = 0UL; idx < BUFFER_SIZE; idx++)
        {
            printf("%d ", SRC_BUFFER[idx]);
        }
        printf("\r\n");

        printf("**************** Destination(SRAM): ****************\r\n");
        for (int idx = 0UL; idx < BUFFER_SIZE; idx++)
        {
            printf("%d ", au8DestBuffer[idx]);
        }
        printf("\r\n");
        printf("Wait for next memory transfer. \r\n");
        printf("\r\n");
    }
}

/* [] END OF FILE */
