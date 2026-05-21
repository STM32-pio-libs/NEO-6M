#include "neo_6m.h"

#include <stddef.h>
#include <string.h>

static void neo6m_clear_value_updates(NEO6M_HandleTypeDef *gps) {
    gps->location.updated = 0U;
    gps->date.updated = 0U;
    gps->time.updated = 0U;
    gps->satellites.updated = 0U;
    gps->hdop_x100.updated = 0U;
    gps->speed_knots_x100.updated = 0U;
    gps->course_deg_x100.updated = 0U;
    gps->altitude_cm.updated = 0U;
    gps->fix_quality.updated = 0U;
}

static uint8_t neo6m_hex_value(char ch, uint8_t *value) {
    if ((ch >= '0') && (ch <= '9')) {
        *value = (uint8_t)(ch - '0');
        return 1U;
    }

    if ((ch >= 'A') && (ch <= 'F')) {
        *value = (uint8_t)(10 + (ch - 'A'));
        return 1U;
    }

    if ((ch >= 'a') && (ch <= 'f')) {
        *value = (uint8_t)(10 + (ch - 'a'));
        return 1U;
    }

    return 0U;
}

static uint8_t neo6m_parse_uint32(const char *text, uint32_t *value) {
    uint32_t result = 0U;
    size_t index = 0U;

    if ((text == NULL) || (text[0] == '\0') || (value == NULL)) {
        return 0U;
    }

    while (text[index] != '\0') {
        if ((text[index] < '0') || (text[index] > '9')) {
            return 0U;
        }

        result = (result * 10U) + (uint32_t)(text[index] - '0');
        index++;
    }

    *value = result;
    return 1U;
}

static uint8_t neo6m_parse_decimal_scaled(const char *text, uint32_t scale, uint32_t *value) {
    uint32_t integer_part = 0U;
    uint32_t fraction_part = 0U;
    uint32_t scale_factor = scale;
    const char *cursor;

    if ((text == NULL) || (text[0] == '\0') || (value == NULL)) {
        return 0U;
    }

    cursor = text;
    while ((*cursor != '\0') && (*cursor != '.')) {
        if ((*cursor < '0') || (*cursor > '9')) {
            return 0U;
        }

        integer_part = (integer_part * 10U) + (uint32_t)(*cursor - '0');
        cursor++;
    }

    if (*cursor == '.') {
        cursor++;
        while ((*cursor != '\0') && (scale_factor > 1U)) {
            if ((*cursor < '0') || (*cursor > '9')) {
                return 0U;
            }

            scale_factor /= 10U;
            fraction_part += (uint32_t)(*cursor - '0') * scale_factor;
            cursor++;
        }

        while (*cursor != '\0') {
            if ((*cursor < '0') || (*cursor > '9')) {
                return 0U;
            }
            cursor++;
        }
    }

    *value = (integer_part * scale) + fraction_part;
    return 1U;
}

static uint8_t neo6m_parse_time(const char *text, NEO6M_Time *time) {
    size_t index;
    uint16_t milliseconds = 0U;
    uint16_t factor = 100U;

    if ((text == NULL) || (time == NULL) || (strlen(text) < 6U)) {
        return 0U;
    }

    if ((text[0] < '0') || (text[0] > '9') ||
        (text[1] < '0') || (text[1] > '9') ||
        (text[2] < '0') || (text[2] > '9') ||
        (text[3] < '0') || (text[3] > '9') ||
        (text[4] < '0') || (text[4] > '9') ||
        (text[5] < '0') || (text[5] > '9'))
    {
        return 0U;
    }

    time->hour = (uint8_t)(((text[0] - '0') * 10) + (text[1] - '0'));
    time->minute = (uint8_t)(((text[2] - '0') * 10) + (text[3] - '0'));
    time->second = (uint8_t)(((text[4] - '0') * 10) + (text[5] - '0'));
    time->milliseconds = 0U;

    if (text[6] == '.') {
        for (index = 7U; (text[index] != '\0') && (factor > 0U); index++) {
            if ((text[index] < '0') || (text[index] > '9')) {
                return 0U;
            }

            milliseconds += (uint16_t)(text[index] - '0') * factor;
            factor = (uint16_t)(factor / 10U);
        }

        while (text[index] != '\0') {
            if ((text[index] < '0') || (text[index] > '9')) {
                return 0U;
            }
            index++;
        }

        time->milliseconds = milliseconds;
    }

    time->valid = 1U;
    time->updated = 1U;
    return 1U;
}

static uint8_t neo6m_parse_date(const char *text, NEO6M_Date *date) {
    uint8_t year_short;

    if ((text == NULL) || (date == NULL) || (strlen(text) != 6U)) {
        return 0U;
    }

    if ((text[0] < '0') || (text[0] > '9') ||
        (text[1] < '0') || (text[1] > '9') ||
        (text[2] < '0') || (text[2] > '9') ||
        (text[3] < '0') || (text[3] > '9') ||
        (text[4] < '0') || (text[4] > '9') ||
        (text[5] < '0') || (text[5] > '9'))
    {
        return 0U;
    }

    date->day = (uint8_t)(((text[0] - '0') * 10) + (text[1] - '0'));
    date->month = (uint8_t)(((text[2] - '0') * 10) + (text[3] - '0'));
    year_short = (uint8_t)(((text[4] - '0') * 10) + (text[5] - '0'));
    date->year = (uint16_t)((year_short >= 80U) ? (1900U + year_short) : (2000U + year_short));
    date->valid = 1U;
    date->updated = 1U;
    return 1U;
}

static uint8_t neo6m_parse_coordinate(const char *text, char hemisphere, int32_t *value_deg_e7) {
    size_t length;
    size_t dot_index = 0U;
    size_t degree_digits;
    size_t index;
    uint32_t degrees = 0U;
    uint32_t minute_scaled_1e6 = 0U;
    uint32_t fraction_scale = 1000000U;
    int64_t result;

    if ((text == NULL) || (value_deg_e7 == NULL) || (text[0] == '\0')) {
        return 0U;
    }

    length = strlen(text);
    for (index = 0U; index < length; index++) {
        if (text[index] == '.') {
            dot_index = index;
            break;
        }
    }

    if ((dot_index < 3U) || (dot_index >= length)) {
        return 0U;
    }

    degree_digits = dot_index - 2U;
    for (index = 0U; index < degree_digits; index++) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return 0U;
        }
        degrees = (degrees * 10U) + (uint32_t)(text[index] - '0');
    }

    for (index = degree_digits; index < dot_index; index++) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return 0U;
        }
        minute_scaled_1e6 = (minute_scaled_1e6 * 10U) + (uint32_t)(text[index] - '0');
    }
    minute_scaled_1e6 *= 1000000U;

    for (index = dot_index + 1U; index < length; index++) {
        if ((text[index] < '0') || (text[index] > '9')) {
            return 0U;
        }

        if (fraction_scale > 1U) {
            fraction_scale /= 10U;
            minute_scaled_1e6 += (uint32_t)(text[index] - '0') * fraction_scale;
        }
    }

    result = ((int64_t)degrees * 10000000LL) + ((int64_t)minute_scaled_1e6 / 6LL);

    if ((hemisphere == 'S') || (hemisphere == 'W')) {
        result = -result;
    }
    else if ((hemisphere != 'N') && (hemisphere != 'E')) {
        return 0U;
    }

    *value_deg_e7 = (int32_t)result;
    return 1U;
}

static uint8_t neo6m_split_fields(char *sentence, char *fields[], size_t max_fields, size_t *field_count) {
    size_t count = 0U;
    char *cursor = sentence;

    if ((sentence == NULL) || (fields == NULL) || (field_count == NULL) || (max_fields == 0U)) {
        return 0U;
    }

    fields[count++] = cursor;
    while ((*cursor != '\0') && (count < max_fields)) {
        if (*cursor == ',') {
            *cursor = '\0';
            fields[count++] = cursor + 1;
        }
        cursor++;
    }

    *field_count = count;
    return 1U;
}

static void neo6m_apply_rmc(NEO6M_HandleTypeDef *gps, char *fields[], size_t field_count) {
    int32_t latitude = 0;
    int32_t longitude = 0;
    uint32_t speed_knots_x100 = 0U;
    uint32_t course_deg_x100 = 0U;

    if ((gps == NULL) || (fields == NULL) || (field_count < 10U)) {
        return;
    }

    if (fields[1][0] != '\0') {
        (void)neo6m_parse_time(fields[1], &gps->time);
    }

    gps->has_fix = (uint8_t)((fields[2][0] == 'A') ? 1U : 0U);

    if ((fields[3][0] != '\0') && (fields[4][0] != '\0') &&
        (fields[5][0] != '\0') && (fields[6][0] != '\0') &&
        (neo6m_parse_coordinate(fields[3], fields[4][0], &latitude) != 0U) &&
        (neo6m_parse_coordinate(fields[5], fields[6][0], &longitude) != 0U))
    {
        gps->location.latitude_deg_e7 = latitude;
        gps->location.longitude_deg_e7 = longitude;
        gps->location.valid = 1U;
        gps->location.updated = 1U;
    }

    if (fields[7][0] != '\0') {
        if (neo6m_parse_decimal_scaled(fields[7], 100U, &speed_knots_x100) != 0U) {
            gps->speed_knots_x100.value = speed_knots_x100;
            gps->speed_knots_x100.valid = 1U;
            gps->speed_knots_x100.updated = 1U;
        }
    }

    if (fields[8][0] != '\0') {
        if (neo6m_parse_decimal_scaled(fields[8], 100U, &course_deg_x100) != 0U) {
            gps->course_deg_x100.value = course_deg_x100;
            gps->course_deg_x100.valid = 1U;
            gps->course_deg_x100.updated = 1U;
        }
    }

    if (fields[9][0] != '\0') {
        (void)neo6m_parse_date(fields[9], &gps->date);
    }
}

static void neo6m_apply_gga(NEO6M_HandleTypeDef *gps, char *fields[], size_t field_count) {
    int32_t latitude = 0;
    int32_t longitude = 0;
    uint32_t fix_quality = 0U;
    uint32_t satellites = 0U;
    uint32_t hdop_x100 = 0U;
    uint32_t altitude_cm = 0U;

    if ((gps == NULL) || (fields == NULL) || (field_count < 10U)) {
        return;
    }

    if (fields[1][0] != '\0') {
        (void)neo6m_parse_time(fields[1], &gps->time);
    }

    if ((fields[2][0] != '\0') && (fields[3][0] != '\0') &&
        (fields[4][0] != '\0') && (fields[5][0] != '\0') &&
        (neo6m_parse_coordinate(fields[2], fields[3][0], &latitude) != 0U) &&
        (neo6m_parse_coordinate(fields[4], fields[5][0], &longitude) != 0U))
    {
        gps->location.latitude_deg_e7 = latitude;
        gps->location.longitude_deg_e7 = longitude;
        gps->location.valid = 1U;
        gps->location.updated = 1U;
    }

    if ((fields[6][0] != '\0') && (neo6m_parse_uint32(fields[6], &fix_quality) != 0U)) {
        gps->fix_quality.value = fix_quality;
        gps->fix_quality.valid = 1U;
        gps->fix_quality.updated = 1U;
        gps->has_fix = (uint8_t)((fix_quality > 0U) ? 1U : 0U);
    }

    if ((fields[7][0] != '\0') && (neo6m_parse_uint32(fields[7], &satellites) != 0U)) {
        gps->satellites.value = satellites;
        gps->satellites.valid = 1U;
        gps->satellites.updated = 1U;
    }

    if ((fields[8][0] != '\0') && (neo6m_parse_decimal_scaled(fields[8], 100U, &hdop_x100) != 0U)) {
        gps->hdop_x100.value = hdop_x100;
        gps->hdop_x100.valid = 1U;
        gps->hdop_x100.updated = 1U;
    }

    if ((fields[9][0] != '\0') && (neo6m_parse_decimal_scaled(fields[9], 100U, &altitude_cm) != 0U)) {
        gps->altitude_cm.value = (int32_t)altitude_cm;
        gps->altitude_cm.valid = 1U;
        gps->altitude_cm.updated = 1U;
    }
}

static uint8_t neo6m_parse_sentence(NEO6M_HandleTypeDef *gps) {
    char buffer[NEO6M_MAX_SENTENCE_LEN + 1U];
    char *fields[20];
    size_t field_count = 0U;

    if (gps == NULL) {
        return 0U;
    }

    if (gps->sentence_length == 0U) {
        return 0U;
    }

    memcpy(buffer, gps->last_sentence, gps->sentence_length + 1U);

    if (neo6m_split_fields(buffer, fields, (sizeof(fields) / sizeof(fields[0])), &field_count) == 0U) {
        return 0U;
    }

    if ((field_count > 0U) &&
        ((strcmp(fields[0], "GPRMC") == 0) || (strcmp(fields[0], "GNRMC") == 0)))
    {
        neo6m_apply_rmc(gps, fields, field_count);
        return 1U;
    }

    if ((field_count > 0U) &&
        ((strcmp(fields[0], "GPGGA") == 0) || (strcmp(fields[0], "GNGGA") == 0)))
    {
        neo6m_apply_gga(gps, fields, field_count);
        return 1U;
    }

    return 0U;
}

void NEO6M_Init(NEO6M_HandleTypeDef *gps) {
    if (gps == NULL) {
        return;
    }

    memset(gps, 0, sizeof(*gps));
}

void NEO6M_ResetParser(NEO6M_HandleTypeDef *gps) {
    if (gps == NULL) {
        return;
    }

    gps->sentence_ready = 0U;
    gps->sentence_length = 0U;
    gps->collecting = 0U;
    gps->checksum = 0U;
    gps->expected_checksum = 0U;
    gps->checksum_digits = 0U;
    gps->checksum_phase = 0U;
    gps->last_sentence[0] = '\0';
}

void NEO6M_ClearUpdates(NEO6M_HandleTypeDef *gps) {
    if (gps == NULL) {
        return;
    }

    neo6m_clear_value_updates(gps);
}

uint8_t NEO6M_Encode(NEO6M_HandleTypeDef *gps, char byte) {
    uint8_t nibble = 0U;

    if (gps == NULL) {
        return 0U;
    }

    if (byte == '$') {
        NEO6M_ResetParser(gps);
        gps->collecting = 1U;
        return 0U;
    }

    if (gps->collecting == 0U) {
        return 0U;
    }

    if ((byte == '\r') || (byte == '\n')) {
        if ((gps->checksum_phase != 0U) &&
            (gps->checksum_digits == 2U) &&
            (gps->expected_checksum == gps->checksum))
        {
            gps->last_sentence[gps->sentence_length] = '\0';
            gps->sentence_ready = 1U;
            gps->sentences++;
            neo6m_clear_value_updates(gps);
            gps->collecting = 0U;
            return neo6m_parse_sentence(gps);
        }

        if (gps->checksum_phase != 0U) {
            gps->checksum_failures++;
        }
        gps->collecting = 0U;
        return 0U;
    }

    if (byte == '*') {
        gps->checksum_phase = 1U;
        gps->expected_checksum = 0U;
        gps->checksum_digits = 0U;
        return 0U;
    }

    if (gps->checksum_phase != 0U) {
        if ((neo6m_hex_value(byte, &nibble) == 0U) || (gps->checksum_digits >= 2U)) {
            gps->collecting = 0U;
            return 0U;
        }

        gps->expected_checksum = (uint8_t)((gps->expected_checksum << 4U) | nibble);
        gps->checksum_digits++;
        return 0U;
    }

    if (gps->sentence_length >= NEO6M_MAX_SENTENCE_LEN) {
        gps->collecting = 0U;
        return 0U;
    }

    gps->last_sentence[gps->sentence_length++] = byte;
    gps->checksum ^= (uint8_t)byte;
    return 0U;
}

uint8_t NEO6M_LocationValid(const NEO6M_HandleTypeDef *gps) {
    if (gps == NULL) {
        return 0U;
    }

    return gps->location.valid;
}

double NEO6M_LatitudeDeg(const NEO6M_HandleTypeDef *gps) {
    if ((gps == NULL) || (gps->location.valid == 0U)) {
        return 0.0;
    }

    return (double)gps->location.latitude_deg_e7 / 10000000.0;
}

double NEO6M_LongitudeDeg(const NEO6M_HandleTypeDef *gps) {
    if ((gps == NULL) || (gps->location.valid == 0U)) {
        return 0.0;
    }

    return (double)gps->location.longitude_deg_e7 / 10000000.0;
}

int32_t NEO6M_AttachUart(NEO6M_HandleTypeDef *gps, const NEO6M_UartConfig *config) {
    if ((gps == NULL) || (config == NULL) || (config->receive_it_fn == NULL)) {
        return NEO6M_ERR_INVALID_ARG;
    }

    gps->uart = *config;
    return NEO6M_OK;
}

int32_t NEO6M_StartReceiveIT(NEO6M_HandleTypeDef *gps) {
    if ((gps == NULL) || (gps->uart.receive_it_fn == NULL)) {
        return NEO6M_ERR_INVALID_ARG;
    }

    return gps->uart.receive_it_fn(gps->uart.user_context, &gps->rx_byte, 1U);
}

int32_t NEO6M_RxCpltCallback(NEO6M_HandleTypeDef *gps) {
    int32_t status;

    if (gps == NULL) {
        return NEO6M_ERR_INVALID_ARG;
    }

    (void)NEO6M_Encode(gps, (char)gps->rx_byte);

    if (gps->uart.receive_it_fn == NULL) {
        return NEO6M_ERR_INVALID_ARG;
    }

    status = gps->uart.receive_it_fn(gps->uart.user_context, &gps->rx_byte, 1U);
    return status;
}
