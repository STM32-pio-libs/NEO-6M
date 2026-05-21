#include "neo_6m.h"
#include "main.h"

UART_HandleTypeDef huart2;
NEO6M_HandleTypeDef gps;

static int32_t neo6m_receive_it(void *user_context, uint8_t *data, size_t length) {
    UART_HandleTypeDef *huart = (UART_HandleTypeDef *)user_context;

    if ((huart == NULL) || (data == NULL) || (length == 0U)) {
        return NEO6M_ERR_INVALID_ARG;
    }

    if (HAL_UART_Receive_IT(huart, data, (uint16_t)length) != HAL_OK) {
        return NEO6M_ERR_IO;
    }

    return NEO6M_OK;
}

static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void) {
    /*
     * Configure the USART2 pins for your MCU and board here.
     *
     * Important:
     * - GPS only needs RX on the STM32 side.
     * - On many STM32 parts USART2 RX is PA3 and TX is PA2.
     * - If your exact MCU exposes USART2 on PA3/PA4, use that alternate
     *   function mapping instead. Verify it against the datasheet first.
     */
}

int main(void) {
    NEO6M_UartConfig uart_config = {
        .user_context = &huart2,
        .receive_it_fn = neo6m_receive_it
    };

    HAL_Init();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    NEO6M_Init(&gps);

    if (NEO6M_AttachUart(&gps, &uart_config) != NEO6M_OK) {
        Error_Handler();
    }

    if (NEO6M_StartReceiveIT(&gps) != NEO6M_OK) {
        Error_Handler();
    }

    while (1) {
        if (gps.location.updated != 0U) {
            /*
             * A full valid sentence updated the decoded fix.
             * Example:
             * double lat = NEO6M_LatitudeDeg(&gps);
             * double lon = NEO6M_LongitudeDeg(&gps);
             */
            NEO6M_ClearUpdates(&gps);
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        if (NEO6M_RxCpltCallback(&gps) != NEO6M_OK) {
            Error_Handler();
        }
    }
}
