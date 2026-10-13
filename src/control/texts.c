/* See texts.h. */
#include "texts.h"

static const char *const TEXTS[][2] = {
#include "page_texts.inc"
};

_Static_assert(sizeof(TEXTS) / sizeof(TEXTS[0]) == TX_COUNT,
               "page_texts.inc and texts.h differ");

const char *tx(int id, int lang)
{
    if (id < 0 || id >= TX_COUNT)
        return "?";
    return TEXTS[id][lang ? 1 : 0];
}
