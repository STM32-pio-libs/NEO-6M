#include "neo_6m.h"

void neo6m_encode(NEO_6M_HandleTypeDef* gps){
    char c;
    if(HAL_UART_Receive(gps->huart, &c, sizeof(char), HAL_MAX_DELAY) == HAL_OK){
        if(gps->message_ok){
            gps->message_ok = false;
            gps->message_length = 0;
        }
        if(c == '\r');
        else if(c == '\n'){
            gps->message[gps->message_length] = '\0';  
            gps->message_ok = true;
        }
        else
            gps->message[gps->message_length++] = c;
    }
}
