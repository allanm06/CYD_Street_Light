/*
 * ui_wifi.c - Tela LVGL (v9) de busca de redes Wi-Fi para a CYD (320x240).
 *
 * Fontes usadas: Montserrat 12, 14 e 18 (habilitar no menuconfig do LVGL).
 */
#include "ui_wifi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define UI_WIFI_MAX_APS 32

/* ---- Paleta ------------------------------------------------------------ */
#define COL_BG       lv_color_hex(0x0F172A)
#define COL_HEADER   lv_color_hex(0x111C33)
#define COL_CARD     lv_color_hex(0x1E293B)
#define COL_CARD_PR  lv_color_hex(0x2B3A52)
#define COL_BORDER   lv_color_hex(0x334155)
#define COL_TEXT     lv_color_hex(0xE2E8F0)
#define COL_MUTED    lv_color_hex(0x94A3B8)
#define COL_ACCENT   lv_color_hex(0x38BDF8)
#define COL_GOOD     lv_color_hex(0x22C55E)
#define COL_FAIR     lv_color_hex(0xEAB308)
#define COL_WEAK     lv_color_hex(0xEF4444)
#define COL_BAR_OFF  lv_color_hex(0x3B4A60)

/* ---- Estado ------------------------------------------------------------ */
static struct {
    lv_obj_t *scr;
    lv_obj_t *status_lbl;
    lv_obj_t *scan_btn;
    lv_obj_t *list;
    lv_obj_t *overlay;      /* spinner / vazio / erro */
    lv_obj_t *overlay_lbl;
    lv_obj_t *spinner;
    lv_obj_t *detail;       /* modal de detalhe */

    ui_wifi_ap_t aps[UI_WIFI_MAX_APS];
    uint16_t ap_count;

    ui_wifi_scan_cb_t scan_cb;
    ui_wifi_connect_cb_t connect_cb;
} s;

/* ---- Helpers ----------------------------------------------------------- */
static const char *auth_str(ui_wifi_auth_t a)
{
    switch (a) {
    case UI_WIFI_AUTH_OPEN:       return "Aberta";
    case UI_WIFI_AUTH_WEP:        return "WEP";
    case UI_WIFI_AUTH_WPA:        return "WPA";
    case UI_WIFI_AUTH_WPA2:       return "WPA2";
    case UI_WIFI_AUTH_WPA_WPA2:   return "WPA/WPA2";
    case UI_WIFI_AUTH_WPA3:       return "WPA3";
    case UI_WIFI_AUTH_WPA2_WPA3:  return "WPA2/WPA3";
    case UI_WIFI_AUTH_ENTERPRISE: return "Enterprise";
    default:                      return "Outra";
    }
}

/* 0..4 barras */
static int rssi_level(int8_t rssi)
{
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    if (rssi >= -90) return 1;
    return 0;
}

static lv_color_t level_color(int lvl)
{
    if (lvl >= 3) return COL_GOOD;
    if (lvl == 2) return COL_FAIR;
    return COL_WEAK;
}

static const char *ssid_or_hidden(const ui_wifi_ap_t *ap)
{
    return ap->ssid[0] ? ap->ssid : "(rede oculta)";
}

static void style_plain(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

/* Icone de sinal: 4 barras verticais crescentes. */
static lv_obj_t *signal_bars_create(lv_obj_t *parent, int8_t rssi)
{
    static const int32_t heights[4] = {5, 9, 13, 17};
    int lvl = rssi_level(rssi);
    lv_color_t on = level_color(lvl);

    lv_obj_t *box = lv_obj_create(parent);
    style_plain(box);
    lv_obj_set_size(box, 22, 17);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(box);
        style_plain(b);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(b, 4, heights[i]);
        lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, i * 6, 0);
        lv_obj_set_style_radius(b, 1, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, i < lvl ? on : COL_BAR_OFF, 0);
    }
    return box;
}

/* Cadeado desenhado (a fonte de simbolos do LVGL nao tem um). */
static lv_obj_t *lock_icon_create(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    style_plain(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(box, 12, 15);

    lv_obj_t *shackle = lv_obj_create(box);
    style_plain(shackle);
    lv_obj_remove_flag(shackle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(shackle, 8, 10);
    lv_obj_align(shackle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(shackle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(shackle, 2, 0);
    lv_obj_set_style_border_color(shackle, COL_MUTED, 0);

    lv_obj_t *body = lv_obj_create(box);
    style_plain(body);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(body, 12, 8);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(body, 2, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(body, COL_MUTED, 0);
    return box;
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

/* ---- Overlay (buscando / vazio / erro) --------------------------------- */
static void overlay_show(bool spinning, const char *text)
{
    lv_obj_remove_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
    if (spinning) lv_obj_remove_flag(s.spinner, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(s.spinner, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s.overlay_lbl, text);
}

static void overlay_hide(void)
{
    lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
}

/* ---- Modal de detalhe -------------------------------------------------- */
static void detail_close(void)
{
    if (s.detail) {
        lv_obj_delete(s.detail);
        s.detail = NULL;
    }
}

static void detail_close_cb(lv_event_t *e)
{
    (void)e;
    detail_close();
}

static void detail_backdrop_cb(lv_event_t *e)
{
    /* fecha somente se o toque foi no fundo, nao no card */
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) detail_close();
}

static void detail_connect_cb(lv_event_t *e)
{
    const ui_wifi_ap_t *ap = lv_event_get_user_data(e);
    ui_wifi_ap_t copy = *ap;    /* detail_close nao invalida s.aps, mas por seguranca */
    detail_close();
    if (s.connect_cb) s.connect_cb(&copy);
}

static void detail_row(lv_obj_t *parent, const char *key, const char *val)
{
    lv_obj_t *row = lv_obj_create(parent);
    style_plain(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    label_create(row, &lv_font_montserrat_12, COL_MUTED, key);
    lv_obj_t *v = label_create(row, &lv_font_montserrat_12, COL_TEXT, val);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, 0);
}

static void detail_open(const ui_wifi_ap_t *ap)
{
    detail_close();

    s.detail = lv_obj_create(s.scr);
    style_plain(s.detail);
    lv_obj_set_size(s.detail, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s.detail, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s.detail, LV_OPA_60, 0);
    lv_obj_add_flag(s.detail, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s.detail, detail_backdrop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *card = lv_obj_create(s.detail);
    style_plain(card);
    lv_obj_set_size(card, 260, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);   /* absorve o toque */
    lv_obj_set_style_bg_color(card, COL_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_color(card, COL_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    /* titulo: barras + ssid */
    lv_obj_t *title = lv_obj_create(card);
    style_plain(title);
    lv_obj_set_size(title, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(title, 8, 0);
    signal_bars_create(title, ap->rssi);
    lv_obj_t *t = label_create(title, &lv_font_montserrat_14, COL_TEXT, ssid_or_hidden(ap));
    lv_obj_set_flex_grow(t, 1);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);

    char buf[32];
    detail_row(card, "Seguranca", auth_str(ap->auth));
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X",
             ap->bssid[0], ap->bssid[1], ap->bssid[2],
             ap->bssid[3], ap->bssid[4], ap->bssid[5]);
    detail_row(card, "BSSID", buf);

    /* botoes */
    lv_obj_t *btns = lv_obj_create(card);
    style_plain(btns);
    lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(btns, 6, 0);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btns, 8, 0);

    lv_obj_t *b_close = lv_button_create(btns);
    lv_obj_set_style_bg_color(b_close, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(b_close, 0, 0);
    lv_obj_set_height(b_close, 32);
    lv_obj_add_event_cb(b_close, detail_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(label_create(b_close, &lv_font_montserrat_14, COL_TEXT, "Fechar"));

    lv_obj_t *b_conn = lv_button_create(btns);
    lv_obj_set_style_bg_color(b_conn, COL_ACCENT, 0);
    lv_obj_set_style_shadow_width(b_conn, 0, 0);
    lv_obj_set_height(b_conn, 32);
    lv_obj_add_event_cb(b_conn, detail_connect_cb, LV_EVENT_CLICKED, (void *)ap);
    lv_obj_center(label_create(b_conn, &lv_font_montserrat_14, COL_BG, "Conectar"));
}

/* ---- Lista ------------------------------------------------------------- */
static void item_clicked_cb(lv_event_t *e)
{
    const ui_wifi_ap_t *ap = lv_event_get_user_data(e);
    detail_open(ap);
}

static void list_add_item(const ui_wifi_ap_t *ap)
{
    lv_obj_t *it = lv_obj_create(s.list);
    style_plain(it);
    lv_obj_set_size(it, lv_pct(100), 44);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(it, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(it, COL_CARD, 0);
    lv_obj_set_style_bg_color(it, COL_CARD_PR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(it, 8, 0);
    lv_obj_set_style_pad_hor(it, 10, 0);
    lv_obj_set_flex_flow(it, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(it, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(it, 10, 0);
    lv_obj_add_event_cb(it, item_clicked_cb, LV_EVENT_CLICKED, (void *)ap);

    signal_bars_create(it, ap->rssi);

    /* coluna de texto */
    lv_obj_t *col = lv_obj_create(it);
    style_plain(col);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 1, 0);

    lv_obj_t *name = label_create(col, &lv_font_montserrat_14,
                                  ap->ssid[0] ? COL_TEXT : COL_MUTED, ssid_or_hidden(ap));
    lv_obj_set_width(name, lv_pct(100));
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);

    label_create(col, &lv_font_montserrat_12, COL_MUTED, auth_str(ap->auth));

    /* cadeado (rede protegida) */
    if (ap->auth != UI_WIFI_AUTH_OPEN) lock_icon_create(it);
}

static int ap_cmp_rssi(const void *a, const void *b)
{
    return ((const ui_wifi_ap_t *)b)->rssi - ((const ui_wifi_ap_t *)a)->rssi;
}

/* ---- Header ------------------------------------------------------------ */
static void scan_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_wifi_scan_started();
    if (s.scan_cb) s.scan_cb();
}

static void header_create(void)
{
    lv_obj_t *hdr = lv_obj_create(s.scr);
    style_plain(hdr);
    lv_obj_set_size(hdr, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hdr, COL_HEADER, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, COL_BORDER, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_pad_hor(hdr, 10, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, 8, 0);

    label_create(hdr, &lv_font_montserrat_18, COL_ACCENT, LV_SYMBOL_WIFI);
    lv_obj_t *title = label_create(hdr, &lv_font_montserrat_18, COL_TEXT, "Redes Wi-Fi");
    lv_obj_set_flex_grow(title, 1);

    s.status_lbl = label_create(hdr, &lv_font_montserrat_12, COL_MUTED, "");

    s.scan_btn = lv_button_create(hdr);
    lv_obj_set_size(s.scan_btn, 34, 30);
    lv_obj_set_style_radius(s.scan_btn, 8, 0);
    lv_obj_set_style_shadow_width(s.scan_btn, 0, 0);
    lv_obj_set_style_bg_color(s.scan_btn, COL_ACCENT, 0);
    lv_obj_set_style_bg_color(s.scan_btn, COL_BORDER, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s.scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(label_create(s.scan_btn, &lv_font_montserrat_14, COL_BG, LV_SYMBOL_REFRESH));
}

/* ---- API publica ------------------------------------------------------- */
void ui_wifi_create(void)
{
    memset(&s, 0, sizeof s);

    s.scr = lv_screen_active();
    lv_obj_clean(s.scr);
    lv_obj_remove_flag(s.scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s.scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s.scr, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s.scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s.scr, 0, 0);
    lv_obj_set_style_pad_row(s.scr, 0, 0);

    header_create();

    /* area de conteudo: lista + overlay sobrepostos */
    lv_obj_t *body = lv_obj_create(s.scr);
    style_plain(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);

    s.list = lv_obj_create(body);
    style_plain(s.list);
    lv_obj_add_flag(s.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s.list, LV_DIR_VER);
    lv_obj_set_size(s.list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(s.list, 6, 0);
    lv_obj_set_style_pad_row(s.list, 5, 0);
    lv_obj_set_flex_flow(s.list, LV_FLEX_FLOW_COLUMN);

    s.overlay = lv_obj_create(body);
    style_plain(s.overlay);
    lv_obj_set_size(s.overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s.overlay, COL_BG, 0);
    lv_obj_set_style_bg_opa(s.overlay, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s.overlay, 12, 0);

    s.spinner = lv_spinner_create(s.overlay);
    lv_obj_set_size(s.spinner, 44, 44);
    lv_obj_set_style_arc_width(s.spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s.spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s.spinner, COL_BAR_OFF, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s.spinner, COL_ACCENT, LV_PART_INDICATOR);

    s.overlay_lbl = label_create(s.overlay, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_set_style_text_align(s.overlay_lbl, LV_TEXT_ALIGN_CENTER, 0);

    overlay_show(false, "Toque em " LV_SYMBOL_REFRESH " para buscar redes");
}

void ui_wifi_set_scan_cb(ui_wifi_scan_cb_t cb)       { s.scan_cb = cb; }
void ui_wifi_set_connect_cb(ui_wifi_connect_cb_t cb) { s.connect_cb = cb; }

void ui_wifi_scan_started(void)
{
    detail_close();
    lv_obj_add_state(s.scan_btn, LV_STATE_DISABLED);
    lv_label_set_text(s.status_lbl, "Buscando...");
    overlay_show(true, "Buscando redes...");
}

void ui_wifi_show_results(const ui_wifi_ap_t *aps, uint16_t count)
{
    detail_close();
    if (count > UI_WIFI_MAX_APS) count = UI_WIFI_MAX_APS;
    memcpy(s.aps, aps, count * sizeof *aps);
    s.ap_count = count;
    qsort(s.aps, count, sizeof *s.aps, ap_cmp_rssi);

    lv_obj_clean(s.list);
    for (uint16_t i = 0; i < count; i++) list_add_item(&s.aps[i]);
    lv_obj_scroll_to_y(s.list, 0, LV_ANIM_OFF);

    lv_obj_remove_state(s.scan_btn, LV_STATE_DISABLED);
    if (count == 0) {
        lv_label_set_text(s.status_lbl, "");
        overlay_show(false, LV_SYMBOL_WARNING "\nNenhuma rede encontrada");
    } else {
        lv_label_set_text_fmt(s.status_lbl, "%u %s", count, count == 1 ? "rede" : "redes");
        overlay_hide();
    }
}

void ui_wifi_show_error(const char *msg)
{
    detail_close();
    lv_obj_remove_state(s.scan_btn, LV_STATE_DISABLED);
    lv_label_set_text(s.status_lbl, "Erro");
    lv_obj_clean(s.list);
    s.ap_count = 0;
    char buf[96];
    snprintf(buf, sizeof buf, LV_SYMBOL_CLOSE "\n%s", msg ? msg : "Falha na busca");
    overlay_show(false, buf);
}
