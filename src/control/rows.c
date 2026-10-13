/* See rows.h. */
#include "rows.h"

int rows_apply(uint16_t *screen, const uint8_t *data, size_t len, rows_dirty_t *dirty)
{
    uint16_t line[SCR_W];
    size_t p = 0;
    int count = 0;

    while (p < len) {
        if (len - p < 2)
            return ROWS_ERR_SHORT;
        int y = data[p];
        int mode = data[p + 1];
        p += 2;
        if (y >= SCR_H)
            return ROWS_ERR_Y;

        if (mode == 0) {
            if (len - p < SCR_W * 2)
                return ROWS_ERR_SHORT;
            for (int x = 0; x < SCR_W; x++, p += 2)
                line[x] = (uint16_t)(data[p] << 8 | data[p + 1]);
        } else if (mode == 1) {
            int x = 0;
            while (x < SCR_W) {
                if (len - p < 3)
                    return ROWS_ERR_SHORT;
                int n = data[p];
                uint16_t v = (uint16_t)(data[p + 1] << 8 | data[p + 2]);
                p += 3;
                if (n == 0 || x + n > SCR_W)
                    return ROWS_ERR_RUN;
                for (int k = 0; k < n; k++)
                    line[x++] = v;
            }
        } else {
            return ROWS_ERR_MODE;
        }

        uint16_t *dst = screen + y * SCR_W;
        for (int x = 0; x < SCR_W; x++)
            dst[x] = line[x];
        dirty->bits[y >> 5] |= 1u << (y & 31);
        count++;
    }
    return count;
}
