/******************************************************************************
 * File Name:   main.c
 *
 * Description: This example demonstartes AT command application
 * that accepts AT command from a Host over UART to configure, manage, 
 * and control Wi-Fi functionalities and features of a device.
 *
 * Related Document: See README.md
 *
 *
 ********************************************************************************
 * Copyright 2019-2023, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *******************************************************************************/

/* Header file includes. */
#include <at_command_app.h>
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cyabs_rtos.h"
#include "tx_api.h"
#include "tx_initialize.h"
#include "cy_log.h"
#include <unistd.h>
/* TCP client task header file. */

/* Include serial flash library and QSPI memory configurations only for the
 * kits that require the Wi-Fi firmware to be loaded in external QSPI NOR flash.
 */

/*******************************************************************************
 * Macros
 ********************************************************************************/
/* RTOS related macros. */
#define TASK_STACK_SIZE (1024 * 8)
#define TASK_PRIORITY (CY_RTOS_PRIORITY_NORMAL)
static uint64_t task_stack[TASK_STACK_SIZE / 8];

/*******************************************************************************
 * Global Variables
 ********************************************************************************/
cy_thread_t task_handle;

static int cy_log_output_handler(CY_LOG_FACILITY_T facility, CY_LOG_LEVEL_T level, char *logmsg)
{
    (void)facility;
    (void)level;

#ifdef __ICCARM__
    __write(_LLIO_STDOUT, (const unsigned char *)logmsg, strlen(logmsg));
#else
    printf("%s\n\r", logmsg);
#endif
    return 0;
}

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  System entrance point. This function sets up user tasks and then starts
 *  the RTOS scheduler.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main()
{
    cy_rslt_t result;

    /* Initialize the board support package. */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

     cyhal_syspm_lock_deepsleep();

    /* To avoid compiler warnings. */
    (void)result;

    /* Enable global interrupts. */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port. */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    /* Initialize the User LED. */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                    CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    cy_log_init(CY_LOG_INFO, cy_log_output_handler, NULL);
    cy_log_set_facility_level(CYLF_MIDDLEWARE, CY_LOG_DEBUG);
    cy_log_set_all_levels(CY_LOG_MAX);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("============================================================\n");
    printf("AT command reference application\n");
    printf("============================================================\n\n");

    result = cy_rtos_thread_create(&task_handle,
                                   &client_task,
                                   "client_task",
                                   &task_stack,
                                   TASK_STACK_SIZE,
                                   TASK_PRIORITY,
                                   0);

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}

/* [] END OF FILE */
