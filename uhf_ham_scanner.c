#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mifare_classic/mifare_classic.h>
#include <string.h>
#include <stdio.h>
#include "font_10x10_cn.h"

#define BLOCK_DATA_LEN 16
#define HEX_STR_LEN (BLOCK_DATA_LEN * 2 + 1)

typedef enum {
    State_WaitCard,
    State_ReadCard,
    State_BrowseSector,
    State_EditBlock,
} AppState;

typedef struct {
    Nfc* nfc;
    NfcDevice* dev;
    AppState state;
    uint8_t current_sector;
    uint8_t current_block;
    uint8_t block_data[BLOCK_DATA_LEN];
    char block_hex[HEX_STR_LEN];
    char sector_desc[32];
} NfcAnalyzerApp;

static NfcAnalyzerApp* nfc_analyzer_alloc(void) {
    NfcAnalyzerApp* app = malloc(sizeof(NfcAnalyzerApp));
    memset(app, 0, sizeof(NfcAnalyzerApp));
    app->nfc = nfc_alloc();
    return app;
}

static void nfc_analyzer_free(NfcAnalyzerApp* app) {
    if(app->dev) {
        nfc_device_free(app->dev);
    }
    nfc_free(app->nfc);
    free(app);
}

// 自动识别扇区功能中文描述
static void get_sector_desc(NfcAnalyzerApp* app, uint8_t sector) {
    if(sector == 0) {
        strncpy(app->sector_desc, "厂商扇区(UID+厂商信息)", sizeof(app->sector_desc));
    } else if((sector & 0x03) == 3) {
        strncpy(app->sector_desc, "密钥&权限控制扇区", sizeof(app->sector_desc));
    } else {
        strncpy(app->sector_desc, "用户数据扇区", sizeof(app->sector_desc));
    }
}

// 二进制转十六进制字符串
static void bin2hex(uint8_t* bin, char* hex, uint8_t len) {
    const char tbl[] = "0123456789ABCDEF";
    for(uint8_t i = 0; i < len; i++) {
        hex[i*2] = tbl[(bin[i] >> 4) & 0x0F];
        hex[i*2 + 1] = tbl[bin[i] & 0x0F];
    }
    hex[len * 2] = '\0';
}

static void render_callback(Canvas* canvas, void* ctx) {
    NfcAnalyzerApp* app = ctx;
    canvas_clear(canvas);
    switch(app->state) {
        case State_WaitCard:
            canvas_draw_str(canvas, 0, 12, "NFC卡片分析器");
            canvas_draw_str(canvas, 0, 26, "贴入Mifare卡");
            canvas_draw_str(canvas, 0, 40, "等待卡片...");
            break;
        case State_ReadCard:
            canvas_draw_str(canvas,0,12,"正在读取卡片");
            canvas_draw_str(canvas,0,26,"请勿移开卡片");
            break;
        case State_BrowseSector:
            canvas_draw_str(canvas,0,10,"NFC卡片分析");
            get_sector_desc(app, app->current_sector);
            canvas_draw_str(canvas,0,22, app->sector_desc);
            char line[32];
            snprintf(line, sizeof(line), "扇区:%d 块:%d", app->current_sector, app->current_block);
            canvas_draw_str(canvas,0,34, line);
            canvas_draw_str(canvas,0,46, app->block_hex);
            canvas_draw_str(canvas,0,58,"OK编辑 | ↑↓切换");
            break;
        case State_EditBlock:
            canvas_draw_str(canvas,0,10,"编辑块数据");
            canvas_draw_str(canvas,0,24, app->block_hex);
            canvas_draw_str(canvas,0,40,"OK写入校验 | BACK返回");
            break;
    }
}

static void input_callback(InputEvent* event, void* ctx) {
    NfcAnalyzerApp* app = ctx;
    if(event->type != InputTypePress) return;

    if(event->key == InputKeyBack) {
        if(app->state == State_EditBlock) {
            app->state = State_BrowseSector;
        } else {
            app->state = State_WaitCard;
            if(app->dev) {
                nfc_device_free(app->dev);
                app->dev = NULL;
            }
        }
        return;
    }

    if(app->state == State_WaitCard) {
        if(nfc_poll(app->nfc, &app->dev, 100)) {
            app->state = State_ReadCard;
            app->current_sector = 0;
            app->current_block = 0;
            nfc_device_load_mifare_classic(app->dev);
            nfc_mifare_classic_read_block(app->dev, app->current_sector, app->current_block, app->block_data);
            bin2hex(app->block_data, app->block_hex, BLOCK_DATA_LEN);
            app->state = State_BrowseSector;
        }
    } else if(app->state == State_BrowseSector) {
        if(event->key == InputKeyUp) {
            app->current_block++;
        } else if(event->key == InputKeyDown) {
            if(app->current_block > 0) app->current_block--;
        } else if(event->key == InputKeyOk) {
            app->state = State_EditBlock;
        }
        nfc_mifare_classic_read_block(app->dev, app->current_sector, app->current_block, app->block_data);
        bin2hex(app->block_data, app->block_hex, BLOCK_DATA_LEN);
    } else if(app->state == State_EditBlock) {
        if(event->key == InputKeyOk) {
            bool write_ok = nfc_mifare_classic_write_block(app->dev, app->current_sector, app->current_block, app->block_data);
            uint8_t verify_buf[BLOCK_DATA_LEN];
            nfc_mifare_classic_read_block(app->dev, app->current_sector, app->current_block, verify_buf);
            // 写入矫正校验，对比回读数据
            if(memcmp(verify_buf, app->block_data, BLOCK_DATA_LEN) == 0 && write_ok) {
                // 写入校验成功
            }
            bin2hex(verify_buf, app->block_hex, BLOCK_DATA_LEN);
            app->state = State_BrowseSector;
        }
    }
}

int32_t nfc_analyzer_app(void* p) {
    UNUSED(p);
    NfcAnalyzerApp* app = nfc_analyzer_alloc();
    ViewPort* view_port = view_port_alloc();
    view_port_set_render_callback(view_port, render_callback, app);
    view_port_set_input_callback(view_port, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(1) {
        view_port_update(view_port);
        furi_delay_ms(50);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    nfc_analyzer_free(app);
    return 0;
}
