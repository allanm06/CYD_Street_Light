/*
 * ui_wifi.c - Tela LVGL (v9) de busca e conexao Wi-Fi para a CYD (320x240).
 *
 * Fontes usadas: Montserrat 12, 14 e 18. As fontes embutidas do LVGL so tem
 * ASCII, por isso os textos nao tem acentos.
 */
#include "ui_wifi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define UI_WIFI_MAX_APS 32
#define UI_WIFI_PASS_MAX 64

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
    lv_obj_t *overlay;      /* spinner / vazio / erro da lista */
    lv_obj_t *overlay_lbl;
    lv_obj_t *spinner;

    lv_obj_t *detail;       /* modal de detalhe da rede */
    lv_obj_t *pwd;          /* tela de senha */
    lv_obj_t *pwd_ta;
    lv_obj_t *pwd_hint;
    lv_obj_t *pwd_eye_lbl;
    lv_obj_t *busy;         /* conectando / resultado */

    ui_wifi_ap_t aps[UI_WIFI_MAX_APS];
    uint16_t ap_count;

    ui_wifi_ap_t target;    /* rede da tentativa de conexao atual */
    char connected_ssid[33];

    ui_wifi_scan_cb_t scan_cb;
    ui_wifi_connect_cb_t connect_cb;
} s;

static void pwd_open(const ui_wifi_ap_t *ap);
static void list_render(void);

/* ======================================================================== */
/* Helpers                                                                  */
/* ======================================================================== */
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

static bool ap_is_connected(const ui_wifi_ap_t *ap)
{
    return ap->ssid[0] && strcmp(ap->ssid, s.connected_ssid) == 0;
}

/* Motivo pelo qual nao da para conectar pela UI, ou NULL se da. */
static const char *ap_unsupported(const ui_wifi_ap_t *ap)
{
    if (!ap->ssid[0]) return "Rede oculta";
    if (ap->auth == UI_WIFI_AUTH_ENTERPRISE) return "Enterprise";
    if (ap->auth == UI_WIFI_AUTH_OTHER) return "Nao suportada";
    return NULL;
}

static void style_plain(lv_obj_t *o)
{
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

/* Painel que cobre a tela inteira (usado por senha / conectando). */
static lv_obj_t *fullscreen_panel(void)
{
    lv_obj_t *p = lv_obj_create(s.scr);
    style_plain(p);
    lv_obj_add_flag(p, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);      /* absorve toques */
    lv_obj_set_size(p, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(p, COL_BG, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    return p;
}

/* Deleta um painel com seguranca mesmo de dentro de um evento dele. */
static void panel_close(lv_obj_t **p)
{
    if (*p) {
        lv_obj_delete_async(*p);
        *p = NULL;
    }
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

static lv_obj_t *button_create(lv_obj_t *parent, const char *txt, bool primary,
                               lv_event_cb_t cb, void *user)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height(b, 32);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_bg_color(b, primary ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_bg_color(b, COL_CARD, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(b, primary ? COL_BG : COL_TEXT, 0);
    lv_obj_set_style_text_color(b, COL_MUTED, LV_STATE_DISABLED);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_14, 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

static lv_obj_t *row_create(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *r = lv_obj_create(parent);
    style_plain(r);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 8, 0);
    return r;
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

/* Barra de topo: [voltar] titulo ... */
static lv_obj_t *header_bar_create(lv_obj_t *parent)
{
    lv_obj_t *hdr = lv_obj_create(parent);
    style_plain(hdr);
    lv_obj_set_size(hdr, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hdr, COL_HEADER, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, COL_BORDER, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_pad_hor(hdr, 8, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, 8, 0);
    return hdr;
}

/* ======================================================================== */
/* Overlay da lista (buscando / vazio / erro)                               */
/* ======================================================================== */
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

/* ======================================================================== */
/* Conectando / resultado                                                   */
/* ======================================================================== */
static void start_connect(const char *password)
{
    panel_close(&s.pwd);
    panel_close(&s.busy);

    s.busy = fullscreen_panel();
    lv_obj_set_flex_flow(s.busy, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.busy, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s.busy, 12, 0);
    lv_obj_set_style_pad_hor(s.busy, 16, 0);

    lv_obj_t *sp = lv_spinner_create(s.busy);
    lv_obj_set_size(sp, 44, 44);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sp, COL_BAR_OFF, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, COL_ACCENT, LV_PART_INDICATOR);

    label_create(s.busy, &lv_font_montserrat_14, COL_MUTED, "Conectando a");
    lv_obj_t *n = label_create(s.busy, &lv_font_montserrat_18, COL_TEXT, s.target.ssid);
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);

    if (s.connect_cb) s.connect_cb(&s.target, password ? password : "");
}

static void result_ok_cb(lv_event_t *e)
{
    (void)e;
    panel_close(&s.busy);
}

static void result_retry_cb(lv_event_t *e)
{
    (void)e;
    panel_close(&s.busy);
    if (s.target.auth == UI_WIFI_AUTH_OPEN) start_connect("");
    else                                    pwd_open(&s.target);
}

/* ======================================================================== */
/* Tela de senha                                                            */
/* ======================================================================== */
static const char *pwd_validate(const char *p, ui_wifi_auth_t auth)
{
    size_t n = strlen(p);
    if (auth == UI_WIFI_AUTH_WEP) {
        if (n == 5 || n == 13 || n == 10 || n == 26) return NULL;
        return "WEP: use 5 ou 13 caracteres";
    }
    if (n < 8) return "A senha precisa ter 8 ou mais caracteres";
    if (n > 63) return "A senha pode ter no maximo 63 caracteres";
    return NULL;
}

static void pwd_hint_set(const char *txt, bool error)
{
    lv_label_set_text(s.pwd_hint, txt);
    lv_obj_set_style_text_color(s.pwd_hint, error ? COL_WEAK : COL_MUTED, 0);
}

static void pwd_submit(void)
{
    const char *p = lv_textarea_get_text(s.pwd_ta);
    const char *err = pwd_validate(p, s.target.auth);
    if (err) {
        pwd_hint_set(err, true);
        return;
    }
    char pass[UI_WIFI_PASS_MAX + 1];
    strncpy(pass, p, UI_WIFI_PASS_MAX);
    pass[UI_WIFI_PASS_MAX] = '\0';
    start_connect(pass);
}

static void pwd_back_cb(lv_event_t *e)
{
    (void)e;
    panel_close(&s.pwd);
}

static void pwd_connect_cb(lv_event_t *e)
{
    (void)e;
    pwd_submit();
}

static void pwd_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY)       pwd_submit();
    else if (code == LV_EVENT_CANCEL) panel_close(&s.pwd);
}

static void pwd_ta_changed_cb(lv_event_t *e)
{
    (void)e;
    if (s.pwd_hint) pwd_hint_set(s.target.auth == UI_WIFI_AUTH_WEP
                                 ? "Senha WEP (5 ou 13 caracteres)"
                                 : "Senha da rede (8 a 63 caracteres)", false);
}

static void pwd_eye_cb(lv_event_t *e)
{
    (void)e;
    bool hidden = lv_textarea_get_password_mode(s.pwd_ta);
    lv_textarea_set_password_mode(s.pwd_ta, !hidden);
    lv_label_set_text(s.pwd_eye_lbl, hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

static void keyboard_style(lv_obj_t *kb)
{
    lv_obj_set_style_bg_color(kb, COL_HEADER, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_radius(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 3, 0);
    lv_obj_set_style_pad_gap(kb, 3, 0);

    lv_obj_set_style_bg_color(kb, COL_CARD, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, COL_CARD_PR, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(kb, COL_BORDER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, COL_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 5, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
}

static void pwd_open(const ui_wifi_ap_t *ap)
{
    if (ap != &s.target) s.target = *ap;
    panel_close(&s.detail);
    panel_close(&s.pwd);

    s.pwd = fullscreen_panel();
    lv_obj_set_flex_flow(s.pwd, LV_FLEX_FLOW_COLUMN);

    /* topo: voltar + nome da rede */
    lv_obj_t *hdr = header_bar_create(s.pwd);
    lv_obj_t *back = button_create(hdr, LV_SYMBOL_LEFT, false, pwd_back_cb, NULL);
    lv_obj_set_size(back, 34, 30);
    lv_obj_t *t = label_create(hdr, &lv_font_montserrat_14, COL_TEXT, s.target.ssid);
    lv_obj_set_flex_grow(t, 1);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t *go = button_create(hdr, "Conectar", true, pwd_connect_cb, NULL);
    lv_obj_set_height(go, 30);

    /* campo de senha + olho */
    lv_obj_t *row = row_create(s.pwd, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_top(row, 6, 0);
    lv_obj_set_style_pad_column(row, 6, 0);

    s.pwd_ta = lv_textarea_create(row);
    lv_obj_set_flex_grow(s.pwd_ta, 1);
    lv_obj_set_height(s.pwd_ta, 36);
    lv_textarea_set_one_line(s.pwd_ta, true);
    lv_textarea_set_password_mode(s.pwd_ta, true);
    lv_textarea_set_max_length(s.pwd_ta, UI_WIFI_PASS_MAX);
    lv_textarea_set_placeholder_text(s.pwd_ta, "Senha");
    lv_obj_set_style_bg_color(s.pwd_ta, COL_CARD, 0);
    lv_obj_set_style_border_color(s.pwd_ta, COL_BORDER, 0);
    lv_obj_set_style_border_color(s.pwd_ta, COL_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(s.pwd_ta, 1, 0);
    lv_obj_set_style_radius(s.pwd_ta, 8, 0);
    lv_obj_set_style_text_color(s.pwd_ta, COL_TEXT, 0);
    lv_obj_set_style_text_font(s.pwd_ta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_ver(s.pwd_ta, 8, 0);
    lv_obj_set_style_text_color(s.pwd_ta, COL_MUTED, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(s.pwd_ta, COL_ACCENT, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_add_state(s.pwd_ta, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(s.pwd_ta, pwd_ta_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *eye = button_create(row, LV_SYMBOL_EYE_OPEN, false, pwd_eye_cb, NULL);
    lv_obj_set_size(eye, 40, 36);
    s.pwd_eye_lbl = lv_obj_get_child(eye, 0);

    s.pwd_hint = label_create(s.pwd, &lv_font_montserrat_12, COL_MUTED, "");
    lv_obj_set_style_pad_hor(s.pwd_hint, 10, 0);
    lv_obj_set_style_pad_ver(s.pwd_hint, 3, 0);
    pwd_ta_changed_cb(NULL);

    /* teclado */
    lv_obj_t *kb = lv_keyboard_create(s.pwd);
    lv_obj_set_width(kb, lv_pct(100));
    lv_obj_set_flex_grow(kb, 1);
    lv_keyboard_set_textarea(kb, s.pwd_ta);
    keyboard_style(kb);
    lv_obj_add_event_cb(kb, pwd_kb_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, pwd_kb_cb, LV_EVENT_CANCEL, NULL);
}

/* ======================================================================== */
/* Modal de detalhe                                                         */
/* ======================================================================== */
static void detail_close_cb(lv_event_t *e)
{
    (void)e;
    panel_close(&s.detail);
}

static void detail_backdrop_cb(lv_event_t *e)
{
    /* fecha somente se o toque foi no fundo, nao no card */
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) panel_close(&s.detail);
}

static void detail_connect_cb(lv_event_t *e)
{
    const ui_wifi_ap_t *ap = lv_event_get_user_data(e);
    s.target = *ap;
    panel_close(&s.detail);
    if (s.target.auth == UI_WIFI_AUTH_OPEN) start_connect("");
    else                                    pwd_open(&s.target);
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
    panel_close(&s.detail);

    s.detail = lv_obj_create(s.scr);
    style_plain(s.detail);
    lv_obj_add_flag(s.detail, LV_OBJ_FLAG_IGNORE_LAYOUT);
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
    lv_obj_t *title = row_create(card, LV_FLEX_ALIGN_START);
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
    if (ap_is_connected(ap)) detail_row(card, "Status", "Conectado");

    /* botoes */
    lv_obj_t *btns = row_create(card, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_top(btns, 6, 0);
    button_create(btns, "Fechar", false, detail_close_cb, NULL);

    const char *why = ap_unsupported(ap);
    lv_obj_t *b = button_create(btns, why ? why : (ap_is_connected(ap) ? "Reconectar" : "Conectar"),
                                true, detail_connect_cb, (void *)ap);
    if (why) lv_obj_add_state(b, LV_STATE_DISABLED);
}

/* ======================================================================== */
/* Lista                                                                    */
/* ======================================================================== */
static void item_clicked_cb(lv_event_t *e)
{
    detail_open(lv_event_get_user_data(e));
}

static void list_add_item(const ui_wifi_ap_t *ap)
{
    bool conn = ap_is_connected(ap);

    lv_obj_t *it = lv_obj_create(s.list);
    style_plain(it);
    lv_obj_set_size(it, lv_pct(100), 44);
    lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(it, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(it, COL_CARD, 0);
    lv_obj_set_style_bg_color(it, COL_CARD_PR, LV_STATE_PRESSED);
    lv_obj_set_style_radius(it, 8, 0);
    if (conn) {
        lv_obj_set_style_border_color(it, COL_GOOD, 0);
        lv_obj_set_style_border_width(it, 1, 0);
    }
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

    if (conn) {
        char sub[40];
        snprintf(sub, sizeof sub, LV_SYMBOL_OK " Conectado  " LV_SYMBOL_BULLET "  %s", auth_str(ap->auth));
        label_create(col, &lv_font_montserrat_12, COL_GOOD, sub);
    } else {
        label_create(col, &lv_font_montserrat_12, COL_MUTED, auth_str(ap->auth));
    }

    if (ap->auth != UI_WIFI_AUTH_OPEN) lock_icon_create(it);
}

/* rede conectada primeiro, depois por sinal */
static int ap_cmp(const void *a, const void *b)
{
    const ui_wifi_ap_t *x = a, *y = b;
    bool cx = ap_is_connected(x), cy = ap_is_connected(y);
    if (cx != cy) return cy - cx;
    return y->rssi - x->rssi;
}

static void list_render(void)
{
    qsort(s.aps, s.ap_count, sizeof *s.aps, ap_cmp);
    lv_obj_clean(s.list);
    for (uint16_t i = 0; i < s.ap_count; i++) list_add_item(&s.aps[i]);
    lv_obj_scroll_to_y(s.list, 0, LV_ANIM_OFF);
}

/* ======================================================================== */
/* Header principal                                                         */
/* ======================================================================== */
static void scan_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_wifi_scan_started();
    if (s.scan_cb) s.scan_cb();
}

static void header_create(void)
{
    lv_obj_t *hdr = header_bar_create(s.scr);
    lv_obj_set_style_pad_hor(hdr, 10, 0);

    label_create(hdr, &lv_font_montserrat_18, COL_ACCENT, LV_SYMBOL_WIFI);
    lv_obj_t *title = label_create(hdr, &lv_font_montserrat_18, COL_TEXT, "Redes Wi-Fi");
    lv_obj_set_flex_grow(title, 1);

    s.status_lbl = label_create(hdr, &lv_font_montserrat_12, COL_MUTED, "");

    s.scan_btn = button_create(hdr, LV_SYMBOL_REFRESH, true, scan_btn_cb, NULL);
    lv_obj_set_size(s.scan_btn, 34, 30);
}

/* ======================================================================== */
/* API publica                                                              */
/* ======================================================================== */
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
    panel_close(&s.detail);
    lv_obj_add_state(s.scan_btn, LV_STATE_DISABLED);
    lv_label_set_text(s.status_lbl, "Buscando...");
    overlay_show(true, "Buscando redes...");
}

void ui_wifi_show_results(const ui_wifi_ap_t *aps, uint16_t count)
{
    panel_close(&s.detail);
    if (count > UI_WIFI_MAX_APS) count = UI_WIFI_MAX_APS;
    if (count) memcpy(s.aps, aps, count * sizeof *aps);
    s.ap_count = count;
    list_render();

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
    panel_close(&s.detail);
    lv_obj_remove_state(s.scan_btn, LV_STATE_DISABLED);
    lv_label_set_text(s.status_lbl, "Erro");
    lv_obj_clean(s.list);
    s.ap_count = 0;
    char buf[96];
    snprintf(buf, sizeof buf, LV_SYMBOL_CLOSE "\n%s", msg ? msg : "Falha na busca");
    overlay_show(false, buf);
}

void ui_wifi_connect_result(bool ok, const char *msg)
{
    if (ok) ui_wifi_set_connected(s.target.ssid);

    panel_close(&s.busy);
    s.busy = fullscreen_panel();
    lv_obj_set_flex_flow(s.busy, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.busy, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s.busy, 8, 0);
    lv_obj_set_style_pad_hor(s.busy, 16, 0);

    /* icone dentro de um circulo */
    lv_obj_t *circle = lv_obj_create(s.busy);
    style_plain(circle);
    lv_obj_set_size(circle, 48, 48);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(circle, ok ? COL_GOOD : COL_WEAK, 0);
    lv_obj_center(label_create(circle, &lv_font_montserrat_18, ok ? COL_GOOD : COL_WEAK,
                               ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE));

    label_create(s.busy, &lv_font_montserrat_18, COL_TEXT,
                 ok ? "Conectado" : "Nao foi possivel conectar");
    lv_obj_t *n = label_create(s.busy, &lv_font_montserrat_14, COL_MUTED, s.target.ssid);
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(n, LV_LABEL_LONG_MODE_DOTS);
    if (msg && msg[0]) label_create(s.busy, &lv_font_montserrat_12, ok ? COL_MUTED : COL_WEAK, msg);

    lv_obj_t *btns = row_create(s.busy, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(btns, 8, 0);
    if (ok) {
        button_create(btns, "OK", true, result_ok_cb, NULL);
    } else {
        button_create(btns, "Voltar", false, result_ok_cb, NULL);
        button_create(btns, "Tentar de novo", true, result_retry_cb, NULL);
    }
}

void ui_wifi_set_connected(const char *ssid)
{
    const char *n = ssid ? ssid : "";
    if (strcmp(n, s.connected_ssid) == 0) return;
    strncpy(s.connected_ssid, n, sizeof s.connected_ssid - 1);
    s.connected_ssid[sizeof s.connected_ssid - 1] = '\0';
    panel_close(&s.detail);     /* o detalhe aponta para itens que vao mudar de posicao */
    list_render();
}
