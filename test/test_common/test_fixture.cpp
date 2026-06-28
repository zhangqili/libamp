#include "test_fixture.h"

#include <cstring>

#include "keyboard.h"

extern "C" {

extern uint8_t flash_buffer[LFS_BLOCK_SIZE * LFS_BLOCK_COUNT];

void libamp_test_clear_output_buffers(void)
{
    std::memset(shared_ep_send_buffer, 0, sizeof(shared_ep_send_buffer));
    std::memset(keyboard_send_buffer, 0, sizeof(keyboard_send_buffer));
    std::memset(raw_send_buffer, 0, sizeof(raw_send_buffer));
    std::memset(midi_send_buffer, 0, sizeof(midi_send_buffer));
    std::memset(led_color_buffer, 0, sizeof(ColorRGB) * RGB_NUM);
    led_flush_count = 0;
    audio_play_note_count = 0;
    audio_stop_note_count = 0;
    audio_stop_all_notes_count = 0;
    audio_last_play_frequency = 0.0f;
    audio_last_stop_frequency = 0.0f;
    audio_last_play_velocity = 0;
    midi_message_callback_count = 0;
    std::memset(&midi_last_message, 0, sizeof(midi_last_message));
}

void libamp_test_reset_environment(void)
{
    std::memset(flash_buffer, 0xFF, LFS_BLOCK_SIZE * LFS_BLOCK_COUNT);
    keyboard_init();
    g_keyboard_config.nkro = false;
    g_keyboard_config.enable_report = true;
    g_keyboard_is_suspend = false;
    g_rgb_hid_mode = false;
    libamp_test_clear_output_buffers();
}

}
