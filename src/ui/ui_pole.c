/*
 * ui_pole.c - Tela principal do Smart Street Light (LVGL 9, 320x240).
 *
 * Layout:
 *   [ Poste 01   * Online                     (wifi) ]  cabecalho
 *   [ (o) Lampada ligada  [=====   ] 80%    12 lux    ]  faixa da lampada
 *   [ Tensao      ][ Corrente    ][ Potencia    ]
 *   [ Fator pot.  ][ Energia     ][ Temperatura ]
 *
 * Fontes: Montserrat 12, 14, 18 e 24 (somente ASCII, sem acentos).
 */
#include "ui_pole.h"
#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

typedef enum { LVL_OK, LVL_WARN, LVL_ALARM } level_t;

typedef struct {
    lv_obj_t *value;
    lv_obj_t *unit;
} tile_t;

enum { T_VOLT, T_CURR, T_POWER, T_PF, T_ENERGY, T_TEMP, T_COUNT };

static struct {
    lv_obj_t *scr;
    lv_obj_t *status_dot;
    lv_obj_t *status_lbl;
    lv_obj_t *wifi_lbl;

    lv_obj_t *lamp_dot;
    lv_obj_t *lamp_lbl;
    lv_obj_t *dim_bar;
    lv_obj_t *dim_lbl;
    lv_obj_t *lux_lbl;

    tile_t tiles[T_COUNT];

    ui_pole_data_t data;
    bool has_data;
    uint32_t last_update;
    int stale_shown;        /* -1 = online, >=0 segundos sem dados mostrados */

    ui_pole_wifi_cb_t wifi_cb;
} s;

/* ======================================================================== */
/* Helpers                                                                  */
/* ======================================================================== */
static void style_plain(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t *dot_create(lv_obj_t *parent, int32_t size, lv_color_t color)
{
    lv_obj_t *d = lv_obj_create(parent);
    style_plain(d);
    lv_obj_set_size(d, size, size);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, color, 0);
    return d;
}

static lv_obj_t *card_create(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    style_plain(c);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_radius(c, 8, 0);
    return c;
}

static lv_color_t level_color(level_t l)
{
    switch (l) {
    case LVL_WARN:  return COL_FAIR;
    case LVL_ALARM: return COL_WEAK;
    default:        return COL_TEXT;
    }
}

/* Faixas de normalidade (rede 220 V, +-10% = 198..242 V). */
static level_t level_voltage(float v)
{
    if (v >= 198.0f && v <= 242.0f) return LVL_OK;
    if (v >= 190.0f && v <= 250.0f) return LVL_WARN;
    return LVL_ALARM;
}

static level_t level_pf(float pf, bool lamp_on)
{
    if (!lamp_on || pf >= 0.92f) return LVL_OK;     /* 0,92 = minimo ANEEL */
    if (pf >= 0.85f) return LVL_WARN;
    return LVL_ALARM;
}

static level_t level_temp(float t)
{
    if (t < 60.0f) return LVL_OK;
    if (t < 75.0f) return LVL_WARN;
    return LVL_ALARM;
}

/* ======================================================================== */
/* Construcao                                                               */
/* ======================================================================== */
static void wifi_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s.wifi_cb) s.wifi_cb();
}

static void header_create(const char *name)
{
    lv_obj_t *hdr = lv_obj_create(s.scr);
    style_plain(hdr);
    lv_obj_set_size(hdr, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hdr, COL_HEADER, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, COL_BORDER, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_pad_left(hdr, 10, 0);
    lv_obj_set_style_pad_right(hdr, 4, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, 6, 0);

    label_create(hdr, &lv_font_montserrat_18, COL_LAMP, LV_SYMBOL_CHARGE);
    label_create(hdr, &lv_font_montserrat_18, COL_TEXT, name);

    /* status dos dados */
    lv_obj_t *pad = lv_obj_create(hdr);
    style_plain(pad);
    lv_obj_set_size(pad, 4, 1);
    s.status_dot = dot_create(hdr, 8, COL_MUTED);
    s.status_lbl = label_create(hdr, &lv_font_montserrat_12, COL_MUTED, "Aguardando dados");
    lv_obj_set_flex_grow(s.status_lbl, 1);

    /* botao Wi-Fi: area de toque maior que o icone */
    lv_obj_t *btn = lv_obj_create(hdr);
    style_plain(btn);
    lv_obj_set_size(btn, 44, 36);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, COL_CARD_PR, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
    s.wifi_lbl = label_create(btn, &lv_font_montserrat_18, COL_WEAK, LV_SYMBOL_WIFI);
    lv_obj_center(s.wifi_lbl);
}

static void lamp_strip_create(lv_obj_t *parent)
{
    lv_obj_t *c = card_create(parent);
    lv_obj_set_size(c, lv_pct(100), 34);
    lv_obj_set_style_pad_hor(c, 10, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 8, 0);

    s.lamp_dot = dot_create(c, 14, COL_BAR_OFF);
    s.lamp_lbl = label_create(c, &lv_font_montserrat_14, COL_TEXT, "Lampada --");

    s.dim_bar = lv_bar_create(c);
    lv_obj_set_size(s.dim_bar, 60, 8);
    lv_bar_set_range(s.dim_bar, 0, 100);
    lv_obj_set_style_bg_color(s.dim_bar, COL_BAR_OFF, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s.dim_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s.dim_bar, COL_LAMP, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s.dim_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s.dim_bar, 4, LV_PART_INDICATOR);

    s.dim_lbl = label_create(c, &lv_font_montserrat_12, COL_MUTED, "--%");
    lv_obj_set_flex_grow(s.dim_lbl, 1);

    s.lux_lbl = label_create(c, &lv_font_montserrat_12, COL_MUTED, "-- lux");
}

static void tile_create(lv_obj_t *grid, int idx, int col, int row,
                        const char *title, const char *unit)
{
    lv_obj_t *c = card_create(grid);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_pad_all(c, 8, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    label_create(c, &lv_font_montserrat_12, COL_MUTED, title);

    lv_obj_t *vrow = lv_obj_create(c);
    style_plain(vrow);
    lv_obj_set_size(vrow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vrow, 3, 0);

    s.tiles[idx].value = label_create(vrow, &lv_font_montserrat_24, COL_MUTED, "--");
    s.tiles[idx].unit = label_create(vrow, &lv_font_montserrat_12, COL_MUTED, unit);
    lv_obj_set_style_pad_bottom(s.tiles[idx].unit, 3, 0);
}

/* ======================================================================== */
/* Atualizacao                                                              */
/* ======================================================================== */
static void tile_set(int idx, const char *txt, level_t lvl, bool stale)
{
    lv_label_set_text(s.tiles[idx].value, txt);
    lv_obj_set_style_text_color(s.tiles[idx].value, stale ? COL_MUTED : level_color(lvl), 0);
}

static void render(bool stale)
{
    const ui_pole_data_t *d = &s.data;
    char buf[24];

    snprintf(buf, sizeof buf, "%.1f", d->voltage_v);
    tile_set(T_VOLT, buf, level_voltage(d->voltage_v), stale);

    snprintf(buf, sizeof buf, "%.2f", d->current_a);
    tile_set(T_CURR, buf, LVL_OK, stale);

    snprintf(buf, sizeof buf, d->power_w < 1000.0f ? "%.1f" : "%.0f", d->power_w);
    tile_set(T_POWER, buf, LVL_OK, stale);

    snprintf(buf, sizeof buf, "%.2f", d->power_factor);
    tile_set(T_PF, buf, level_pf(d->power_factor, d->lamp_on), stale);

    float e = d->energy_kwh;
    snprintf(buf, sizeof buf, e < 100.0f ? "%.2f" : (e < 1000.0f ? "%.1f" : "%.0f"), e);
    tile_set(T_ENERGY, buf, LVL_OK, stale);

    snprintf(buf, sizeof buf, "%.1f", d->temperature_c);
    tile_set(T_TEMP, buf, level_temp(d->temperature_c), stale);

    /* lampada */
    lv_color_t lamp = stale ? COL_MUTED : (d->lamp_on ? COL_LAMP : COL_BAR_OFF);
    lv_obj_set_style_bg_color(s.lamp_dot, lamp, 0);
    lv_obj_set_style_shadow_width(s.lamp_dot, d->lamp_on && !stale ? 10 : 0, 0);
    lv_obj_set_style_shadow_color(s.lamp_dot, COL_LAMP, 0);
    lv_label_set_text(s.lamp_lbl, d->lamp_on ? "Lampada ligada" : "Lampada desligada");
    lv_obj_set_style_text_color(s.lamp_lbl, stale ? COL_MUTED : COL_TEXT, 0);
    lv_bar_set_value(s.dim_bar, d->lamp_on ? d->dimming_pct : 0, LV_ANIM_ON);
    lv_label_set_text_fmt(s.dim_lbl, "%u%%", d->lamp_on ? d->dimming_pct : 0);
    snprintf(buf, sizeof buf, "%.0f lux", d->ambient_lux);
    lv_label_set_text(s.lux_lbl, buf);
}

static void status_set(lv_color_t c, const char *txt)
{
    lv_obj_set_style_bg_color(s.status_dot, c, 0);
    lv_obj_set_style_text_color(s.status_lbl, c, 0);
    lv_label_set_text(s.status_lbl, txt);
}

/* Verifica a idade da ultima leitura. */
static void freshness_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s.has_data) return;

    uint32_t age = lv_tick_elaps(s.last_update);
    if (age < UI_POLE_STALE_MS) {
        if (s.stale_shown != -1) {
            s.stale_shown = -1;
            status_set(COL_GOOD, "Online");
        }
        return;
    }
    int secs = (int)(age / 1000);
    if (secs == s.stale_shown) return;
    if (s.stale_shown == -1) render(true);      /* acabou de ficar sem dados */
    s.stale_shown = secs;
    char buf[32];
    if (secs < 60) snprintf(buf, sizeof buf, "Sem dados ha %ds", secs);
    else           snprintf(buf, sizeof buf, "Sem dados ha %dmin", secs / 60);
    status_set(COL_WEAK, buf);
}

/* ======================================================================== */
/* API publica                                                              */
/* ======================================================================== */
void ui_pole_create(const char *pole_name)
{
    memset(&s, 0, sizeof s);
    s.stale_shown = -2;

    s.scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s.scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s.scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s.scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s.scr, 0, 0);
    lv_obj_set_flex_flow(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s.scr, 0, 0);

    header_create(pole_name ? pole_name : "Poste");

    lv_obj_t *body = lv_obj_create(s.scr);
    style_plain(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_all(body, 6, 0);
    lv_obj_set_style_pad_row(body, 6, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    lamp_strip_create(body);

    static const int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_t *grid = lv_obj_create(body);
    style_plain(grid);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_style_grid_column_dsc_array(grid, cols, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, rows, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);

    tile_create(grid, T_VOLT,   0, 0, "Tensao",      "V");
    tile_create(grid, T_CURR,   1, 0, "Corrente",    "A");
    tile_create(grid, T_POWER,  2, 0, "Potencia",    "W");
    tile_create(grid, T_PF,     0, 1, "Fator pot.",  "");
    tile_create(grid, T_ENERGY, 1, 1, "Energia",     "kWh");
    tile_create(grid, T_TEMP,   2, 1, "Temperatura", "C");

    lv_timer_create(freshness_timer_cb, 500, NULL);
    lv_screen_load(s.scr);
}

void ui_pole_set_wifi_cb(ui_pole_wifi_cb_t cb) { s.wifi_cb = cb; }

void ui_pole_show(void)
{
    lv_screen_load_anim(s.scr, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

void ui_pole_update(const ui_pole_data_t *d)
{
    s.data = *d;
    s.has_data = true;
    s.last_update = lv_tick_get();
    if (s.stale_shown != -1) {
        s.stale_shown = -1;
        status_set(COL_GOOD, "Online");
    }
    render(false);
}

void ui_pole_set_wifi(bool connected, const char *ssid)
{
    (void)ssid;
    lv_obj_set_style_text_color(s.wifi_lbl, connected ? COL_ACCENT : COL_WEAK, 0);
}
