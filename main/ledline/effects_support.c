#include "effects_ledline.h"

//=================================================================
uint32_t color_string_to_uint32(const char* color_str) {
    if (color_str == NULL || strlen(color_str) != 7 || color_str[0] != '#') {
        return 0;
    }

    char hex_str[7];
    strncpy(hex_str, color_str + 1, 6);
    hex_str[6] = '\0';

    char* endptr;
    uint32_t color = strtoul(hex_str, &endptr, 16);

    if (*endptr != '\0') {
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
int compare_rgb(const rgb_t* a, const rgb_t* b)
{
    if (a == NULL || b == NULL) {
        return -1; // Один из указателей равен NULL
    }

    if (a->r != b->r) {
        return 0; // R компоненты не совпадают
    }
    if (a->g != b->g) {
        return 0; // G компоненты не совпадают
    }
    if (a->b != b->b) {
        return 0; // B компоненты не совпадают
    }

    return 1; // Все компоненты совпадают
}