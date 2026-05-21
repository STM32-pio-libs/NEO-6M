#ifndef NEO_6M_H
#define NEO_6M_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEO6M_OK 0
#define NEO6M_ERR_INVALID_ARG (-1)
#define NEO6M_ERR_IO (-2)

#define NEO6M_MAX_SENTENCE_LEN 127U

typedef int32_t (*NEO6M_ReceiveITFn)(void *user_context, uint8_t *data, size_t length);

typedef struct {
    int32_t latitude_deg_e7;
    int32_t longitude_deg_e7;
    uint8_t valid;
    uint8_t updated;
} NEO6M_Location;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t valid;
    uint8_t updated;
} NEO6M_Date;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t milliseconds;
    uint8_t valid;
    uint8_t updated;
} NEO6M_Time;

typedef struct {
    uint32_t value;
    uint8_t valid;
    uint8_t updated;
} NEO6M_UIntValue;

typedef struct {
    int32_t value;
    uint8_t valid;
    uint8_t updated;
} NEO6M_IntValue;

typedef struct {
    void *user_context;
    NEO6M_ReceiveITFn receive_it_fn;
} NEO6M_UartConfig;

typedef struct {
    NEO6M_Location location;
    NEO6M_Date date;
    NEO6M_Time time;
    NEO6M_UIntValue satellites;
    NEO6M_UIntValue hdop_x100;
    NEO6M_UIntValue speed_knots_x100;
    NEO6M_UIntValue course_deg_x100;
    NEO6M_IntValue altitude_cm;
    NEO6M_UIntValue fix_quality;
    uint32_t sentences;
    uint32_t checksum_failures;
    uint8_t has_fix;
    uint8_t sentence_ready;
    char last_sentence[NEO6M_MAX_SENTENCE_LEN + 1U];
    uint8_t sentence_length;
    uint8_t collecting;
    uint8_t checksum;
    uint8_t expected_checksum;
    uint8_t checksum_digits;
    uint8_t checksum_phase;
    uint8_t rx_byte;
    NEO6M_UartConfig uart;
} NEO6M_HandleTypeDef;

void NEO6M_Init(NEO6M_HandleTypeDef *gps);
void NEO6M_ResetParser(NEO6M_HandleTypeDef *gps);
void NEO6M_ClearUpdates(NEO6M_HandleTypeDef *gps);
uint8_t NEO6M_Encode(NEO6M_HandleTypeDef *gps, char byte);
uint8_t NEO6M_LocationValid(const NEO6M_HandleTypeDef *gps);
double NEO6M_LatitudeDeg(const NEO6M_HandleTypeDef *gps);
double NEO6M_LongitudeDeg(const NEO6M_HandleTypeDef *gps);

int32_t NEO6M_AttachUart(NEO6M_HandleTypeDef *gps, const NEO6M_UartConfig *config);
int32_t NEO6M_StartReceiveIT(NEO6M_HandleTypeDef *gps);
int32_t NEO6M_RxCpltCallback(NEO6M_HandleTypeDef *gps);

#ifdef __cplusplus
}
#endif

#endif
