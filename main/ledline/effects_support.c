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
bool color_hsv_interpolate(hsv_t *current, const hsv_t *target, const uint8_t step)
{
    if (!current || !target)
    {
        return false;
    }

    if (color_hsv_equal(current, target) == true)
    {
        return false;
    }

    uint32_t current_rgb = color_from_hsv(*current);
    uint32_t target_rgb = color_from_hsv(*target);

    uint8_t cr = (current_rgb >> 16) & 0xFF;
    uint8_t cg = (current_rgb >> 8) & 0xFF;
    uint8_t cb = current_rgb & 0xFF;

    uint8_t tr = (target_rgb >> 16) & 0xFF;
    uint8_t tg = (target_rgb >> 8) & 0xFF;
    uint8_t tb = target_rgb & 0xFF;

    bool changed = false;

    if (tr > cr)
    {
        if (tr - cr > step)
        {
            cr += step;
            changed = true;
        }
        else
        {
            cr = tr;
            changed = true;
        }
    }
    else if (tr < cr)
    {
        if (cr - tr > step)
        {
            cr -= step;
            changed = true;
        }
        else
        {
            cr = tr;
            changed = true;
        }
    }

    if (tg > cg)
    {
        if (tg - cg > step)
        {
            cg += step;
            changed = true;
        }
        else
        {
            cg = tg;
            changed = true;
        }
    }
    else if (tg < cg)
    {
        if (cg - tg > step)
        {
            cg -= step;
            changed = true;
        }
        else
        {
            cg = tg;
            changed = true;
        }
    }

    if (tb > cb)
    {
        if (tb - cb > step)
        {
            cb += step;
            changed = true;
        }
        else
        {
            cb = tb;
            changed = true;
        }
    }
    else if (tb < cb)
    {
        if (cb - tb > step)
        {
            cb -= step;
            changed = true;
        }
        else
        {
            cb = tb;
            changed = true;
        }
    }

    uint32_t new_rgb = (0xFFU << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;

    hsv_t new_hsv = color_to_hsv(new_rgb);

    if (new_rgb == 0xFF000000)
    {
        new_hsv.val = 0;
        new_hsv.hue = target->hue;
        new_hsv.sat = target->sat;
        changed = true; // Убедимся, что возвращаем true, если RGB стал чёрным
    }

    *current = new_hsv;

    return changed;
}
//=================================================================