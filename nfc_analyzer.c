#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>

static void render(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 20, "Test App OK!");
    canvas_draw_str(canvas, 0, 40, "BACK:Exit");
}
static void input(InputEvent* event, void* ctx) {
    UNUSED(ctx);
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        furi_exit();
    }
}
int32_t nfc_analyzer_app(void* p) {
    UNUSED(p);
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, render, NULL);
    view_port_input_callback_set(vp, input, NULL);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    while(1) {
        view_port_update(vp);
        furi_delay_ms(100);
    }
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    return 0;
}
= furi_record_open(RECORD_GUI);    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    while(1) {
        view_port_update(vp);
        furi_delay_ms(100);
    }
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    return 0;
}
lloc();    app->ir = infrared_alloc();
    strncpy(app->info_text, "Ready, Press OK to start scan", TEXT_BUF_LEN);
    return app;
}

static void rf_scanner_free(RfScannerApp* app) {
    if(!app) return;
    subghz_free(app->subghz);
    nfc_free(app->nfc);
    infrared_free(app->ir);
    free(app);
}

static void render_callback(Canvas* canvas, void* ctx) {
    RfScannerApp* app = ctx;
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 12, "RF Multi-Signal Detector");
    canvas_draw_str(canvas, 0, 30, app->info_text);
    canvas_draw_str(canvas, 0, 50, "BACK: Exit");
}

static void input_callback(InputEvent* event, void* ctx) {
    RfScannerApp* app = ctx;
    if(event->type != InputTypePress) return;

    if(event->key == InputKeyBack) {
        app->state = State_Idle;
        strncpy(app->info_text, "Scan stopped", TEXT_BUF_LEN);
        return;
    }
    if(event->key == InputKeyOk) {
        if(app->state == State_Idle) {
            app->state = State_Scan;
            strncpy(app->info_text, "Scanning RF signals...", TEXT_BUF_LEN);
        } else {
            app->state = State_Idle;
            strncpy(app->info_text, "Scan stopped", TEXT_BUF_LEN);
        }
    }
}

static void scan_loop(RfScannerApp* app) {
    if(app->state != State_Scan) return;

    // NFC 检测
    NfcDevice* dev = NULL;
    if(nfc_poll(app->nfc, &dev, 50)) {
        snprintf(app->info_text, TEXT_BUF_LEN, "Detect: NFC Card");
        nfc_device_free(dev);
        furi_delay_ms(1000);
    }

    // SubGhz 信号检测
    SubGhzReceiver* rx = subghz_receiver_alloc(app->subghz);
    if(subghz_receiver_decode(rx)) {
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mifare_classic/mifare_classic.h>
#include <string.h>
#include <stdio.h>

#define BUF_SIZE 128
#define BLOCK_SIZE 16
#define MAX_SECTOR 16

typedef enum {
    State_Idle,
    State_WaitCard,
    State_ReadSector,
} AppState;

typedef struct {
    AppState state;
    Nfc* nfc;
    NfcDevice* dev;

    uint8_t current_sector;
    MifareClassicKey key_a;
    MifareClassicKey key_b;
    bool key_valid;

    uint8_t block_data[BLOCK_SIZE];
    char text_buf[BUF_SIZE];
} NfcAnalyzerApp;

static NfcAnalyzerApp* nfc_analyzer_alloc(void) {
    NfcAnalyzerApp* app = malloc(sizeof(NfcAnalyzerApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(NfcAnalyzerApp));
    app->nfc = nfc_alloc();
    app->state = State_Idle;
    app->current_sector = 0;
    memset(app->key_a.data, 0xFF, 6);
    memset(app->key_b.data, 0xFF, 6);
    app->key_valid = true;
    strncpy(app->text_buf, "Press OK -> Wait NFC Card", BUF_SIZE);
    return app;
}

static void nfc_analyzer_free(NfcAnalyzerApp* app) {
    if(!app) return;
    if(app->dev) nfc_device_free(app->dev);
    if(app->nfc) nfc_free(app->nfc);
    free(app);
}

static void render_callback(Canvas* canvas, void* ctx) {
    NfcAnalyzerApp* app = ctx;
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 12, "NFC Partition Analyzer");
    canvas_draw_str(canvas, 0, 30, app->text_buf);
    canvas_draw_str(canvas, 0, 48, "UP/DOWN: Switch Sector");
    canvas_draw_str(canvas, 0, 60, "BACK: Exit");
}

static void input_callback(InputEvent* event, void* ctx) {
    NfcAnalyzerApp* app = ctx;
    if(event->type != InputTypePress) return;

    if(event->key == InputKeyBack) {
        app->state = State_Idle;
        if(app->dev) {
            nfc_device_free(app->dev);
            app->dev = NULL;
        }
        strncpy(app->text_buf, "Press OK -> Wait NFC Card", BUF_SIZE);
        return;
    }

    if(event->key == InputKeyOk) {
        if(app->state == State_Idle) {
            app->state = State_WaitCard;
            strncpy(app->text_buf, "Place NFC Card...", BUF_SIZE);
        }
    }

    if(event->key == InputKeyUp && app->state >= State_ReadSector) {
        if(app->current_sector > 0) app->current_sector--;
    }
    if(event->key == InputKeyDown && app->state >= State_ReadSector) {
        if(app->current_sector < MAX_SECTOR - 1) app->current_sector++;
    }
}

static void read_sector_info(NfcAnalyzerApp* app) {
    if(!app->dev) return;
    // 修复：改用 nfc_device_get_protocol_instance，兼容mntm-012
    MifareClassic* mc = nfc_device_get_protocol_instance(app->dev, NfcProtocolMifareClassic);
    if(!mc) {
        strncpy(app->text_buf, "Not Mifare Classic Card", BUF_SIZE);
        return;
    }

    uint16_t block_num = mifare_classic_get_first_block_num_of_sector(app->current_sector);
    bool auth_res = mifare_classic_authenticate(mc, block_num, MifareClassicKeyTypeA, &app->key_a);

    if(!auth_res) {
        snprintf(app->text_buf, BUF_SIZE, "Sector %d: Auth Fail", app->current_sector);
        return;
    }

    bool read_ok = mifare_classic_read_block(mc, block_num, app->block_data);
    if(read_ok) {
        snprintf(app->text_buf, BUF_SIZE, "Sec:%d Blk:%02X Data: %02X%02X...",
                 app->current_sector, block_num, app->block_data[0], app->block_data[1]);
    } else {
        snprintf(app->text_buf, BUF_SIZE, "Sec:%d Read Error", app->current_sector);
    }
}

static void app_loop(NfcAnalyzerApp* app) {
    if(app->state == State_WaitCard) {
        NfcDevice* dev = NULL;
        if(nfc_poll(app->nfc, &dev, 50)) {
            app->dev = dev;
            app->state = State_ReadSector;
            strncpy(app->text_buf, "Card Detected!", BUF_SIZE);
            furi_delay_ms(300);
        }
    }

    if(app->state == State_ReadSector) {
        read_sector_info(app);
        furi_delay_ms(200);
    }
}

int32_t nfc_analyzer_app(void* p) {
    UNUSED(p);
    NfcAnalyzerApp* app = nfc_analyzer_alloc();
    if(!app) return -1;

    ViewPort* vp = view_port_alloc();
    view_port_set_render_callback(vp, render_callback, app);
    view_port_set_input_callback(vp, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    while(1) {
        view_port_update(vp);
        app_loop(app);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    nfc_analyzer_free(app);
    return 0;
}
