
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <subghz/subghz.h>
#include <nfc/nfc.h>
#include <infrared/infrared.h>
#include <string.h>
#include <stdio.h>

#define TEXT_BUF_LEN 64

typedef enum {
    State_Idle,
    State_Scan,
} AppState;

typedef struct {
    AppState state;
    SubGhz* subghz;
    Nfc* nfc;
    Infrared* ir;
    char info_text[TEXT_BUF_LEN];
} RfScannerApp;

static RfScannerApp* rf_scanner_alloc(void) {
    RfScannerApp* app = malloc(sizeof(RfScannerApp));
    if(!app) return NULL;
    memset(app,0,sizeof(RfScannerApp));
    app->subghz = subghz_alloc();
    app->nfc = nfc_alloc();
    app->ir = infrared_alloc();
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
        snprintf(app->info_text, TEXT_BUF_LEN, "Detect: SubGhz Radio");
        furi_delay_ms(1000);
    }
    subghz_receiver_free(rx);

    // IR红外信号检测
    InfraredSignal* ir_sig = infrared_signal_alloc();
    if(infrared_receive(app->ir, ir_sig, 50)) {
        snprintf(app->info_text, TEXT_BUF_LEN, "Detect: Infrared Remote");
        infrared_signal_free(ir_sig);
        furi_delay_ms(1000);
    }
}

int32_t rf_scanner_app(void* p) {
    UNUSED(p);
    RfScannerApp* app = rf_scanner_alloc();
    if(!app) return -1;

    ViewPort* view_port = view_port_alloc();
    view_port_set_render_callback(view_port, render_callback, app);
    view_port_set_input_callback(view_port, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(1) {
        view_port_update(view_port);
        scan_loop(app);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    rf_scanner_free(app);
    return 0;
}
