#include "art_slots.h"

#include "gfx/font_sized.h"
#include "gfx/text.h"
#include "hal/storage.h"
#include "ui/header.h"
#include "ui/label.h"

_Static_assert(sizeof(ArtSlot) == 16, "art slots are stored as 16-byte records");

enum {
    SEGMENT_GAP = 2
};

int art_slots_load(const char *path, uint32_t pixel_bytes, ArtSlot *slots, int capacity)
{
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return 0;
    }
    uint32_t count = file.size > pixel_bytes ? (file.size - pixel_bytes) / sizeof(ArtSlot) : 0;
    count = count < (uint32_t)capacity ? count : (uint32_t)capacity;
    bool read = count > 0 && hal_file_read(file, pixel_bytes, slots, count * sizeof(ArtSlot));
    hal_file_close(&file);
    return read ? (int)count : 0;
}

static int draw_text(Canvas *canvas, const Theme *theme, ArtSlot slot, const char *text, int pen)
{
    const Font *font = slot.font != 0 ? font_sized_code(slot.font) : theme->fonts.small;
    int width = text_width(font, text);
    width = width < slot.width ? width : slot.width;
    int left = slot.after != 0            ? pen + slot.x
               : slot.style == ART_CENTER ? slot.x - width / 2
               : slot.style == ART_RIGHT  ? slot.x - width
                                          : slot.x;
    int height = font_line_height(font);
    label_draw(canvas, (TextStyle){.font = font, .color = slot.color},
               (Label){{left, slot.y - height / 2, width, height}, text});
    return left + width;
}

static void draw_level(Canvas *canvas, const Theme *theme, ArtSlot slot, uint16_t permille)
{
    int filled = slot.width * (permille < ART_PERMILLE ? permille : ART_PERMILLE) / ART_PERMILLE;
    if (slot.style == ART_DISC) {
        header_fill_disc(canvas, (Point){slot.x + filled, slot.y + slot.height / 2},
                         slot.height / 2, slot.color);
    } else if (slot.style == ART_MARK) {
        header_mark(canvas,
                    (Rect){slot.x + filled - slot.height / 2, slot.y, slot.height, slot.height},
                    theme->style, slot.color);
    } else if (slot.font == 0) {
        canvas_fill_rect(canvas, (Rect){slot.x, slot.y, filled, slot.height}, slot.color);
    } else {
        for (int index = 0; index < slot.font && index * slot.width / slot.font < filled; index++) {
            int left = slot.x + index * slot.width / slot.font;
            int right = slot.x + (index + 1) * slot.width / slot.font - SEGMENT_GAP;
            canvas_fill_rect(canvas, (Rect){left, slot.y, right - left, slot.height}, slot.color);
        }
    }
}

void art_slots_draw(Canvas *canvas, const Theme *theme, const ArtSlot *slots, int count,
                    const ArtSlotValues *values)
{
    int pen = 0;
    for (int index = 0; index < count; index++) {
        ArtSlot slot = slots[index];
        if (slot.kind < ART_TEXT_COUNT && slot.style <= ART_RIGHT) {
            const char *text = values->texts[slot.kind];
            if (text != NULL && slot.width > 0) {
                pen = draw_text(canvas, theme, slot, text, pen);
            }
        } else if (slot.kind >= ART_TEXT_COUNT && slot.kind < ART_KIND_COUNT &&
                   slot.style >= ART_BAR && slot.style <= ART_MARK && slot.width > 0) {
            draw_level(canvas, theme, slot, values->permille[slot.kind - ART_TEXT_COUNT]);
        }
    }
}
