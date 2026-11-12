#include "ui/menu_screen.h"

#include <stddef.h>

#include "ui/header.h"
#include "ui/list_navigation.h"

static bool is_shown(const MenuItem *item)
{
    return item->shown == NULL || item->shown(item->shown_context);
}

static int item_count(const void *context)
{
    const MenuScreen *menu = context;
    int count = 0;
    for (int index = 0; index < menu->config.item_count; index++) {
        count += is_shown(&menu->config.items[index]) ? 1 : 0;
    }
    return count;
}

static const MenuItem *shown_item(const MenuScreen *menu, int position)
{
    int remaining = position;
    for (int index = 0; index < menu->config.item_count; index++) {
        const MenuItem *item = &menu->config.items[index];
        if (!is_shown(item)) {
            continue;
        }
        if (remaining == 0) {
            return item;
        }
        remaining--;
    }
    return &menu->config.items[menu->config.item_count - 1];
}

static const char *item_label(const void *context, int index)
{
    return shown_item(context, index)->label;
}

static const char *item_value(const void *context, int index)
{
    const MenuItem *item = shown_item(context, index);
    return item->value != NULL ? item->value(item->context) : NULL;
}

static bool item_opens(const void *context, int index)
{
    return shown_item(context, index)->opens;
}

static ScreenAction select_item(void *context, int index)
{
    const MenuItem *item = shown_item(context, index);
    return item->select != NULL ? item->select(item->context) : screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    MenuScreen *menu = context;
    const MenuPanel *panel = &menu->config.panel;
    if (panel->steer != NULL) {
        ScreenAction steered = screen_action_none();
        if (panel->steer(panel->context, event, now_ms, &steered)) {
            return steered;
        }
    }
    ListSelection selection = {.select = select_item, .hold = NULL, .context = menu};
    return list_navigation_handle(&menu->list, event, now_ms, selection);
}

static void enter(void *context, uint32_t now_ms)
{
    MenuScreen *menu = context;
    menu->now_ms = now_ms;
    list_view_restart_marquee(&menu->list, now_ms);
}

static void update(void *context, uint32_t now_ms)
{
    MenuScreen *menu = context;
    menu->now_ms = now_ms;
    const MenuPanel *panel = &menu->config.panel;
    if (panel->width > 0 && panel->update != NULL) {
        panel->update(panel->context, now_ms);
    }
}

static uint32_t panel_picture(const MenuScreen *menu)
{
    const MenuPanel *panel = &menu->config.panel;
    return panel->picture != NULL ? panel->picture(panel->context) : 0;
}

static uint32_t picture(const void *context)
{
    const MenuScreen *menu = context;
    const MenuPanel *panel = &menu->config.panel;
    uint32_t page = panel_picture(menu);
    if (page != 0 && !panel->follows_selection) {
        return page;
    }
    if (panel->width > 0 && panel->draw != NULL && !panel->follows_selection) {
        return 0;
    }
    uint32_t hash = list_view_picture(&menu->list, theme_content_area(menu->config.theme),
                                      menu->now_ms, menu->config.title);
    if (page != 0) {
        hash = screen_picture_mix(hash, page);
    }
    return hash != 0 ? hash : 1;
}

static void draw(void *context, Canvas *canvas)
{
    const MenuScreen *menu = context;
    const Theme *theme = menu->config.theme;
    const MenuPanel *panel = &menu->config.panel;
    Rect content = theme_content_area(theme);
    if (panel_picture(menu) != 0 && !panel->follows_selection) {
        Rect header = theme_header_area(theme);
        panel->draw(panel->context, canvas,
                    (Rect){header.x, header.y, header.width, header.height + content.height});
        header_draw_bare();
        return;
    }
    header_draw(canvas, theme, menu->config.title);
    if (panel->width <= 0 || panel->draw == NULL) {
        list_view_draw(&menu->list, canvas, content, menu->now_ms);
        return;
    }
    Rect list = {content.x, content.y, content.width - panel->width, content.height};
    Rect side = {list.x + list.width, content.y, panel->width, content.height};
    list_view_draw(&menu->list, canvas, list, menu->now_ms);
    panel->draw(panel->context, canvas, side);
}

Screen *menu_screen_init(MenuScreen *menu, MenuConfig config)
{
    *menu = (MenuScreen){.config = config};
    menu->model = (ListModel){.context = menu,
                              .count = item_count,
                              .label = item_label,
                              .icons = true,
                              .detail = item_value,
                              .opens = item_opens,
                              .two_line = config.two_line};
    list_view_init(&menu->list, &menu->model, config.theme, 0);
    menu->screen = (Screen){
        .title = config.title,
        .context = menu,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
        .picture = picture,
    };
    return &menu->screen;
}

ScreenAction menu_open_screen(void *screen)
{
    return screen_action_push(screen);
}
