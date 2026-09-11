#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <subghz/subghz.h>

const uint32_t scan_freq_list[] = {
    432800000UL,
    432950000UL,
    434500000UL,
    434550000UL,
    434750000UL,
    438450000UL,
    438500000UL,
    438800000UL,
    439500000UL,
    439550000UL,
    439750000UL,
};
#define FREQ_COUNT (sizeof(scan_freq_list)/sizeof(uint32_t))

int32_t uhf_ham_scanner_app(void* p) {
    UNUSED(p);
    SubGhz* subghz = furi_record_open(RECORD_SUBGHZ);
    Gui* gui = furi_record_open(RECORD_GUI);

    ViewDispatcher* view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    TextBox* text_box = text_box_alloc();
    text_box_set_font(text_box, TextBoxFontText);
    view_dispatcher_add_view(view_dispatcher, 0, text_box_get_view(text_box));
    view_dispatcher_switch_to_view(view_dispatcher, 0);

    uint8_t freq_idx = 0;
    bool lock_freq = false;
    uint32_t current_freq = scan_freq_list[0];

    while(1) {
        InputEvent event;
        View* view = view_dispatcher_get_current_view(view_dispatcher);
        if(view_get_input_event(view, &event)) {
            if(event.key == InputKeyOk && event.type == InputTypePress) {
                lock_freq = !lock_freq;
                if(lock_freq) {
                    subghz_set_frequency(subghz, current_freq);
                    subghz_set_modulation(subghz, SubGhzModulationFM238);
                    subghz_start_reception(subghz);
                }
            }
            if(event.key == InputKeyBack && event.type == InputTypePress) {
                break;
            }
        }

        if(!lock_freq) {
            freq_idx = (freq_idx + 1) % FREQ_COUNT;
            current_freq = scan_freq_list[freq_idx];
            subghz_set_frequency(subghz, current_freq);
            subghz_set_modulation(subghz, SubGhzModulationFM238);
            subghz_start_reception(subghz);
            furi_delay_ms(150);
        }

        int rssi = subghz_get_rssi(subghz);
        FuriString* str = furi_string_alloc();
        furi_string_printf(str,
            "70cm 业余频段扫描\n"
            "Freq: %.3f MHz\n"
            "RSSI: %d dBm\n"
            "State: %s\n"
            "OK:锁定频点 | BACK退出\n"
            "Signal: %s",
            (double)current_freq / 1000000.0,
            rssi,
            lock_freq ? "[LOCKED]" : "[SCANNING]",
            (rssi > -70) ? "强信号" : (rssi > -90 ? "弱信号" : "无信号")
        );
        text_box_set_text(text_box, furi_string_get_cstr(str));
        furi_string_free(str);

        subghz_stop_reception(subghz);
        furi_delay_ms(80);
    }

    subghz_stop_reception(subghz);
    view_dispatcher_remove_view(view_dispatcher,0);
    view_dispatcher_free(view_dispatcher);
    text_box_free(text_box);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_GUI);
    return 0;
}
