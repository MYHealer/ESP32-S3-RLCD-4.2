#include "status_gif_contract.h"

static_assert(STATUS_GIF_WIDTH == 84);
static_assert(STATUS_GIF_HEIGHT == 84);
static_assert(STATUS_GIF_FRAME_COUNT == 60);
static_assert(STATUS_GIF_BYTES_PER_FRAME == 882);
static_assert(STATUS_GIF_BYTES_PER_FRAME ==
              (STATUS_GIF_WIDTH * STATUS_GIF_HEIGHT + 7) / 8);

int main()
{
    return 0;
}
