#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_box.h>
#include <subghz/subghz.h>

#define FREQ_433 433920000UL
#define FREQ_868 868350000UL
const uint32_t scan_freq_list[] = {FREQ_433, FREQ_868};
#define FREQ_COUNT (sizeof(scan_freq_list)/sizeof(uint32_t))

int32_t rf_scanner_app(void* p) {
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
            subghz_start_reception(subghz);
            furi_delay_ms(120);
        }

        int rssi = subghz_get_rssi(subghz);
        FuriString* str = furi_string_alloc();
        furi_string_printf(str,
            "RF Scanner(ISM免执照频段)\n"
            "Freq: %.2f MHz\n"
            "RSSI: %d dBm\n"
            "State: %s\n"
            "OK: Lock/Unlock Freq\nBACK: Exit\n"
            "Signal: %s",
            (double)current_freq / 1000000.0,
            rssi,
            lock_freq ? "[LOCKED]" : "[SCANNING]",
            (rssi > -70) ? "STRONG" : (rssi > -90 ? "WEAK" : "NO SIGNAL")
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

    furi_record_close(RECORD_SUBGHZ);
    furi_record_close(RECORD_GUI);
    return 0;
}
