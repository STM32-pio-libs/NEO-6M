#ifndef _NEO_6M_H
#define _NEO_6M_H
#include <main.h>
#include <stdbool.h>

typedef struct {
    UART_HandleTypeDef* huart;
    char message[85];
    uint16_t message_length;
    char** splitted_message;
    uint16_t splitted_length;
    bool message_ok;
}NEO_6M_HandleTypeDef;

void neo6m_encode(NEO_6M_HandleTypeDef* gps);

void splitMessage(NEO_6M_HandleTypeDef* gps);

void freeSplitMessage(NEO_6M_HandleTypeDef* gps);

#endif
