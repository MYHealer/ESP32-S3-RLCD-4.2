// 验证图片时钟内置图库轻量契约的固定尺寸和数量。
#include "clock_gallery_contract.h"

static_assert(CLOCK_GALLERY_IMAGE_WIDTH == 220);
static_assert(CLOCK_GALLERY_IMAGE_HEIGHT == 208);
static_assert(CLOCK_GALLERY_IMAGE_BYTES_PER_ROW == 28);
static_assert(CLOCK_GALLERY_IMAGE_COUNT == 7);
static_assert(CLOCK_GALLERY_IMAGE_DEFAULT_THRESHOLD == 200);
static_assert(CLOCK_GALLERY_IMAGE_DEFAULT_EDGE_FADE_PX == 18);
static_assert(CLOCK_GALLERY_IMAGE_BYTES_PER_ROW ==
              (CLOCK_GALLERY_IMAGE_WIDTH + 7) / 8);

int main()
{
    return 0;
}
