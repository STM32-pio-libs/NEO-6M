#include "neo_6m.h"
#include <string.h>
#include <stdlib.h>


static char* substr(char* raw_str, size_t idx, size_t len){
    for(; raw_str[idx]==' '; idx++,len--);
    for(; raw_str[idx+len-1]==' '; len--);
    char* result = malloc(sizeof(char)*(len+1));
    int i, j;
    for(i=idx, j=0; i<=(len+idx-1) && raw_str[i]!='\0'; i++, j++){
        result[j] = raw_str[i];
    }
    result[j] = '\0';
    return result;
}

void splitMessage(NEO_6M_HandleTypeDef* gps){
    gps->splitted_length = 0;
    for(uint16_t i=0; i<gps->message_length; i++)
        if(gps->message[i]==',')
            gps->splitted_length++;

    gps->splitted_message = calloc(++gps->splitted_length, sizeof(char*));

    for(uint16_t pos=0, idx=0; pos < gps->message_length; pos++, idx++){
        uint16_t i;
        for(i=pos; gps->message[i]!='\0'&&gps->message[i]!=','&&gps->message[i]!='\n'; i++); 
        gps->splitted_message[idx] = substr(gps->message, pos, i-pos);
        pos = i;
    }
    
}

void freeSplitMessage(NEO_6M_HandleTypeDef* gps){
    for(int i=0; i<gps->splitted_length ; i++){
        free(gps->splitted_message[i]);
    }
    free(gps->splitted_message);
}

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
