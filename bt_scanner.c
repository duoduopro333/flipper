#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <bt/bt.h>
#include <furi_ble/profile_interface.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DEVICES 20
#define NAME_MAX_LEN 20
#define MAC_LEN 6

typedef struct {
    uint8_t mac[MAC_LEN];
    char name[NAME_MAX_LEN + 1];
    int8_t rssi;
    uint32_t last_seen;
    bool present;
} BtDevice;

typedef struct {
    ViewPort* view_port;
    Gui* gui;
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    BtDevice devices[MAX_DEVICES];
    int device_count;
    bool scanning;
    uint32_t scan_start_time;
    int selected_index;
    bool running;
} BtScannerApp;

// 蓝牙状态回调
static void bt_status_changed_cb(BtStatus status, void* context) {
    BtScannerApp* app = (BtScannerApp*)context;
    FURI_LOG_I("BTScanner", "BT status changed: %d", status);
    UNUSED(app);
}

// 绘制回调
static void app_draw_callback(Canvas* canvas, void* ctx) {
    BtScannerApp* app = (BtScannerApp*)ctx;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "BLE Scanner");
    
    // 状态行
    canvas_set_font(canvas, FontSecondary);
    if(app->scanning) {
        canvas_draw_str(canvas, 80, 10, "Scanning...");
    } else {
        canvas_draw_str(canvas, 80, 10, "Stopped");
    }
    
    // 分隔线
    canvas_draw_line(canvas, 0, 13, 128, 13);
    
    // 设备列表区域 - 最多显示4个设备
    int display_start = app->selected_index - 1;
    if(display_start < 0) display_start = 0;
    if(app->device_count > 4 && display_start > app->device_count - 4) {
        display_start = app->device_count - 4;
    }
    
    char line_buf[32];
    for(int i = 0; i < 4 && display_start + i < app->device_count; i++) {
        int idx = display_start + i;
        int y = 24 + i * 12;
        
        // 选中标记
        if(idx == app->selected_index) {
            canvas_draw_str(canvas, 2, y, ">");
        }
        
        BtDevice* dev = &app->devices[idx];
        
        // MAC地址
        snprintf(line_buf, sizeof(line_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 dev->mac[0], dev->mac[1], dev->mac[2],
                 dev->mac[3], dev->mac[4], dev->mac[5]);
        canvas_draw_str(canvas, 10, y, line_buf);
        
        // RSSI
        snprintf(line_buf, sizeof(line_buf), "%d", dev->rssi);
        canvas_draw_str(canvas, 100, y, line_buf);
        
        // 设备名称 (第二行)
        if(strlen(dev->name) > 0) {
            canvas_draw_str(canvas, 10, y + 10, dev->name);
        }
    }
    
    // 底部帮助
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "OK:Scan BK:Exit");
}

// 添加或更新设备
static void add_device(BtScannerApp* app, const uint8_t* mac, int8_t rssi, const uint8_t* adv_data, uint8_t adv_len) {
    // 查找是否已存在
    int found = -1;
    for(int i = 0; i < app->device_count; i++) {
        if(memcmp(app->devices[i].mac, mac, MAC_LEN) == 0) {
            found = i;
            break;
        }
    }
    
    if(found >= 0) {
        // 更新现有设备
        app->devices[found].rssi = rssi;
        app->devices[found].last_seen = furi_get_tick();
    } else if(app->device_count < MAX_DEVICES) {
        // 添加新设备
        BtDevice* dev = &app->devices[app->device_count];
        memcpy(dev->mac, mac, MAC_LEN);
        dev->rssi = rssi;
        dev->last_seen = furi_get_tick();
        dev->present = true;
        memset(dev->name, 0, sizeof(dev->name));
        strncpy(dev->name, "Unknown", NAME_MAX_LEN);
        
        // 尝试解析设备名称
        // 简化的ADV解析 - 查找Complete Local Name (0x09) 或 Shortened Local Name (0x08)
        uint8_t pos = 0;
        while(pos < adv_len) {
            uint8_t len = adv_data[pos];
            if(len == 0 || pos + len >= adv_len) break;
            
            uint8_t type = adv_data[pos + 1];
            if(type == 0x08 || type == 0x09) {
                int name_len = len - 1;
                if(name_len > NAME_MAX_LEN) name_len = NAME_MAX_LEN;
                memcpy(dev->name, &adv_data[pos + 2], name_len);
                dev->name[name_len] = '\0';
                break;
            }
            pos += len + 1;
        }
        
        app->device_count++;
    }
}

// 模拟扫描函数 - 在实际蓝牙扫描API可用时，会被真实扫描回调替换
// 官方固件中，被动BLE扫描需要在固件层面支持，第三方FAP可通过
// furi_hal_bt API进行基础蓝牙操作
static void simulate_scan_tick(BtScannerApp* app) {
    // 这是一个演示模式 - 在实际固件支持BLE扫描时，替换为真实扫描回调
    // 此处用于展示UI和应用框架
    UNUSED(app);
}

// 开始/停止扫描
static void toggle_scan(BtScannerApp* app) {
    app->scanning = !app->scanning;
    if(app->scanning) {
        app->device_count = 0;
        app->scan_start_time = furi_get_tick();
        FURI_LOG_I("BTScanner", "Starting BLE scan");
        
        // 确保蓝牙已开启
        if(app->bt) {
            // 蓝牙已初始化，监听状态变化
            bt_set_status_changed_callback(app->bt, bt_status_changed_cb, app);
        }
        
        // 注意：官方OFW目前没有为第三方FAP开放完整的BLE被动扫描API
        // 此应用框架提供完整UI，在支持的固件（如Momentum/Unleashed）上
        // 可以通过扩展furi_hal_bt API实现真实扫描
        // 演示模式：添加示例设备展示效果
        if(app->device_count == 0) {
            // 添加演示设备
            uint8_t demo_mac1[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
            uint8_t demo_adv1[] = {0x07, 0x09, 'F', 'l', 'i', 'p', 'p', 'e', 'r'};
            add_device(app, demo_mac1, -45, demo_adv1, sizeof(demo_adv1));
            
            uint8_t demo_mac2[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
            uint8_t demo_adv2[] = {0x05, 0x09, 'P', 'h', 'o', 'n', 'e'};
            add_device(app, demo_mac2, -62, demo_adv2, sizeof(demo_adv2));
            
            uint8_t demo_mac3[] = {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54};
            add_device(app, demo_mac3, -78, NULL, 0);
        }
    } else {
        FURI_LOG_I("BTScanner", "Stopping BLE scan");
    }
}

// 输入回调
static void app_input_callback(InputEvent* input_event, void* ctx) {
    BtScannerApp* app = (BtScannerApp*)ctx;
    
    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
            case InputKeyUp:
                if(app->selected_index > 0) {
                    app->selected_index--;
                }
                break;
            case InputKeyDown:
                if(app->selected_index < app->device_count - 1) {
                    app->selected_index++;
                }
                break;
            case InputKeyOk:
                toggle_scan(app);
                break;
            case InputKeyBack:
                app->running = false;
                break;
            default:
                break;
        }
        view_port_update(app->view_port);
    }
}

// 消息处理
static void message_loop(BtScannerApp* app) {
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    view_port_input_callback_set(app->view_port, app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    InputEvent event;
    while(app->running) {
        if(app->scanning) {
            simulate_scan_tick(app);
        }
        
        FuriStatus status = furi_message_queue_get(event_queue, &event, 100);
        if(status == FuriStatusOk) {
            app_input_callback(&event, app);
        }
        
        // 更新视图
        view_port_update(app->view_port);
        furi_delay_ms(50);
    }
    
    // 注意：Input callback实际上是通过ViewPort直接分发的
    // 此消息循环作为心跳和扫描定时器使用
    
    gui_remove_view_port(app->gui, app->view_port);
    furi_message_queue_free(event_queue);
}

// 应用入口
int32_t bt_scanner_app(void* p) {
    UNUSED(p);
    
    BtScannerApp* app = malloc(sizeof(BtScannerApp));
    if(!app) return -1;
    
    memset(app, 0, sizeof(BtScannerApp));
    app->running = true;
    app->scanning = false;
    app->selected_index = 0;
    app->device_count = 0;
    
    // 分配GUI资源
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    
    // 打开蓝牙记录
    app->bt = furi_record_open(RECORD_BT);
    
    FURI_LOG_I("BTScanner", "BT Scanner application started");
    
    // 运行主循环 - 使用ViewPort内置事件分发
    // ViewPort会自动处理输入，我们只需要一个轮询循环
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    while(app->running) {
        if(app->scanning) {
            simulate_scan_tick(app);
        }
        
        view_port_update(app->view_port);
        furi_delay_ms(100);
    }
    
    gui_remove_view_port(app->gui, app->view_port);
    
    // 清理
    if(app->ble_profile && app->bt) {
        bt_profile_restore_default(app->bt);
    }
    
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    free(app);
    
    return 0;
}
