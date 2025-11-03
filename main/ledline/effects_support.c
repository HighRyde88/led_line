#include "string.h"
#include "effects_ledline.h"
#include "math.h"

//=================================================================
uint32_t color_from_hex(const char *hex_str)
{
    if (hex_str == NULL || strlen(hex_str) != 7 || hex_str[0] != '#')
    {
        return 0;
    }

    char hex_buf[7];
    strncpy(hex_buf, hex_str + 1, 6);
    hex_buf[6] = '\0';

    char *endptr;
    uint32_t color = strtoul(hex_buf, &endptr, 16);

    if (*endptr != '\0')
    {
        return 0;
    }

    return color & 0x00FFFFFF;
}

//=================================================================
uint8_t split_string(const char *input, char **output_array, int array_size, char delimiter)
{
    if (!input || !output_array || array_size <= 0)
    {
        return 0;
    }

    int count = 0;
    const char *start = input;
    const char *end = input;

    while (*end && count < array_size)
    {
        if (*end == delimiter)
        {
            int len = end - start;
            output_array[count] = malloc(len + 1);
            if (output_array[count] != NULL)
            {
                strncpy(output_array[count], start, len);
                output_array[count][len] = '\0';
                count++;
                start = end + 1;
            }
        }
        end++;
    }

    if (count < array_size && start < end)
    {
        int len = end - start;
        output_array[count] = malloc(len + 1);
        if (output_array[count] != NULL)
        {
            strncpy(output_array[count], start, len);
            output_array[count][len] = '\0';
            count++;
        }
    }

    return count;
}

//=================================================================
hsv_t color_to_hsv(uint32_t color)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max = (rf > gf && rf > bf) ? rf : (gf > bf ? gf : bf);
    float min = (rf < gf && rf < bf) ? rf : (gf < bf ? gf : bf);

    float h, s, v;
    v = max;

    float delta = max - min;
    if (max == 0.0f)
    {
        s = 0;
    }
    else
    {
        s = (delta / max);
    }

    if (delta == 0)
    {
        h = 0;
    }
    else
    {
        if (max == rf)
        {
            h = (gf - bf) / delta;
            if (h < 0)
                h += 6;
        }
        else if (max == gf)
        {
            h = (bf - rf) / delta + 2;
        }
        else
        {
            h = (rf - gf) / delta + 4;
        }
        h *= 60; // degrees
    }

    hsv_t hsv;
    hsv.hue = (uint16_t)(h + 0.5f);
    hsv.sat = (uint8_t)(s * 255 + 0.5f);
    hsv.val = (uint8_t)(v * 255 + 0.5f);

    return hsv;
}

//=================================================================
uint32_t color_from_hsv(hsv_t hsv)
{
    float h = hsv.hue;
    float s = hsv.sat / 255.0f;
    float v = hsv.val / 255.0f;

    float c = v * s; // Chroma
    float h_prime = h / 60.0f;
    float x = c * (1 - fabs(fmod(h_prime, 2) - 1));
    float m = v - c;

    float r_prime, g_prime, b_prime;

    if (0 <= h_prime && h_prime < 1)
    {
        r_prime = c;
        g_prime = x;
        b_prime = 0;
    }
    else if (1 <= h_prime && h_prime < 2)
    {
        r_prime = x;
        g_prime = c;
        b_prime = 0;
    }
    else if (2 <= h_prime && h_prime < 3)
    {
        r_prime = 0;
        g_prime = c;
        b_prime = x;
    }
    else if (3 <= h_prime && h_prime < 4)
    {
        r_prime = 0;
        g_prime = x;
        b_prime = c;
    }
    else if (4 <= h_prime && h_prime < 5)
    {
        r_prime = x;
        g_prime = 0;
        b_prime = c;
    }
    else if (5 <= h_prime && h_prime < 6)
    {
        r_prime = c;
        g_prime = 0;
        b_prime = x;
    }
    else
    {
        r_prime = 0;
        g_prime = 0;
        b_prime = 0; // fallback
    }

    uint8_t r = (uint8_t)((r_prime + m) * 255 + 0.5f);
    uint8_t g = (uint8_t)((g_prime + m) * 255 + 0.5f);
    uint8_t b = (uint8_t)((b_prime + m) * 255 + 0.5f);

    return (0xFFU << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

//=================================================================
bool color_hsv_equal(const hsv_t *a, const hsv_t *b)
{
    if (!a || !b)
    {
        return false;
    }
    return (a->hue == b->hue) && (a->sat == b->sat) && (a->val == b->val);
}
//=================================================================
void hsv_interpolate_step(hsv_t *current, const hsv_t *target, uint16_t total_steps, hsv_interpolation_state_t *state)
{
    if (!current || !target || !state)
    {
        return;
    }

    // Если изменились параметры — сбросить состояние
    if (!color_hsv_equal(&state->target, target) || state->total_steps != total_steps)
    {
        state->start = *current;
        state->target = *target;
        state->total_steps = (total_steps == 0) ? 1 : total_steps;
        state->step = 0;
        state->active = true;
    }

    if (!state->active)
    {
        *current = state->target;
        return;
    }

    if (state->step >= state->total_steps)
    {
        *current = state->target;
        state->active = false;
        return;
    }

    const uint16_t HUE_MAX = 360;

    // Разница по hue с учётом кратчайшего пути
    int16_t hue_diff = (int16_t)state->target.hue - (int16_t)state->start.hue;
    if (hue_diff > 180)
    {
        hue_diff -= HUE_MAX;
    }
    else if (hue_diff < -180)
    {
        hue_diff += HUE_MAX;
    }

    // Прогресс: от 0.0 до 1.0
    float t = (float)(state->step + 1) / state->total_steps;

    // Интерполяция hue
    int32_t new_hue = (int32_t)state->start.hue + (int32_t)(t * hue_diff + 0.5f);
    new_hue = (new_hue % HUE_MAX + HUE_MAX) % HUE_MAX;

    // Интерполяция sat и val
    uint8_t new_sat = (uint8_t)((1.0f - t) * state->start.sat + t * state->target.sat + 0.5f);
    uint8_t new_val = (uint8_t)((1.0f - t) * state->start.val + t * state->target.val + 0.5f);

    current->hue = (uint16_t)new_hue;
    current->sat = new_sat;
    current->val = new_val;

    state->step++;

    // Если интерполяция завершена — фиксируем результат
    if (state->step >= state->total_steps)
    {
        *current = state->target;
        state->active = false;
    }
}
//=================================================================