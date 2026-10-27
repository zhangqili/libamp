#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>

#include "keyboard.h"
#include "layer.h"
#include "nexus.h"
#include "packet.h"

// These tests build nexus.c as a master in bitmap mode (the library default).

extern "C" {
const uint16_t g_nexus_test_slave_map[] = {2, 5, 8};
NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM] = {
    {3, g_nexus_test_slave_map},
};
}

namespace {

constexpr uint32_t kLinkTimeoutTicks = KEYBOARD_TIME_TO_TICK(NEXUS_LINK_TIMEOUT_MS);
constexpr uint32_t kRequestTimeoutTicks = KEYBOARD_TIME_TO_TICK(NEXUS_REQUEST_TIMEOUT_MS);

struct CapturedNexusPacket {
    uint8_t slave_id;
    PacketAdvancedKey packet;
};

CapturedNexusPacket captured_packets[8];
size_t captured_packet_count;
bool captured_decode_ok;
bool synthesize_version_response;
bool corrupt_response_id;
bool drop_echo;
bool fail_send;
void (*down_callback)(Key *);

class ScopedNexusMapping {
public:
    ScopedNexusMapping(const uint16_t *map, uint16_t length)
        : saved_(g_nexus_slave_configs[0])
    {
        g_nexus_slave_configs[0] = {length, map};
        nexus_init();
    }

    ~ScopedNexusMapping()
    {
        g_nexus_slave_configs[0] = saved_;
        nexus_init();
    }

private:
    NexusSlaveKeymap saved_;
};

class ScopedDownCallback {
public:
    explicit ScopedDownCallback(void (*callback)(Key *))
    {
        down_callback = callback;
    }

    ~ScopedDownCallback()
    {
        down_callback = nullptr;
    }
};

void reset_capture()
{
    std::memset(captured_packets, 0, sizeof(captured_packets));
    std::memset(g_nexus_slave_buffer, 0, sizeof(g_nexus_slave_buffer));
    captured_packet_count = 0;
    captured_decode_ok = true;
    synthesize_version_response = false;
    corrupt_response_id = false;
    drop_echo = false;
    fail_send = false;
    g_keyboard_tick = 0;
    nexus_init();
}

void set_test_config(uint16_t key_index)
{
    AdvancedKeyConfiguration *config = &g_keyboard_advanced_keys[key_index].config;
    std::memset(config, 0, sizeof(*config));
    config->mode = ADVANCED_KEY_ANALOG_RAPID_MODE;
    config->activation_value = 1234;
    config->deactivation_value = 567;
    config->trigger_distance = 42;
    config->release_distance = 24;
    config->upper_deadzone = 100;
    config->lower_deadzone = 200;
}

// Deliver a report frame from slave 0 as its transport would.
void deliver_report(uint16_t index, AnalogRawValue raw, AnalogValue value,
                    uint32_t bitmap, uint32_t bitmap_high = 0)
{
    PacketNexus packet;
    volatile uint32_t bits[NEXUS_BITMAP_WORDS] = {bitmap};
#if NEXUS_BITMAP_WORDS > 1
    bits[1] = bitmap_high;
#else
    (void)bitmap_high;
#endif
    nexus_report_encode(&packet, index, raw, value, bits, g_nexus_slave_configs[0].length);
    nexus_process_buffer(0, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
}

// Bring slave 0 online and let nexus_poll() push its initial configuration.
void bring_slave_online_and_drain()
{
    deliver_report(0, 0, 0, 0);
    for (int i = 0; i < 16; i++)
    {
        nexus_poll();
    }
    captured_packet_count = 0;
}

} // namespace

extern "C" void keyboard_key_event_down_callback_user(Key *key)
{
    if (down_callback != nullptr)
    {
        down_callback(key);
    }
}

// The transport stub: capture what the master sends, then behave like a slave
// that processes the packet and echoes it back through nexus_process_buffer().
extern "C" int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len)
{
    if (fail_send)
    {
        return 1;
    }
    if (report == NULL || len == 0 || len > NEXUS_RX_BUFFER_SIZE)
    {
        captured_decode_ok = false;
        return 1;
    }

    if (captured_packet_count < sizeof(captured_packets) / sizeof(captured_packets[0]))
    {
        CapturedNexusPacket *captured = &captured_packets[captured_packet_count++];
        captured->slave_id = slave_id;
        std::memset(&captured->packet, 0, sizeof(captured->packet));
        const size_t capture_len = len < sizeof(captured->packet) ? len : sizeof(captured->packet);
        std::memcpy(&captured->packet, report, capture_len);
    }

    if (drop_echo || report[0] == PACKET_CODE_EVENT)
    {
        return 0;
    }

    uint8_t echo[NEXUS_RX_BUFFER_SIZE] = {0};
    std::memcpy(echo, report, len);
    if (synthesize_version_response && report[2] == PACKET_DATA_VERSION)
    {
        PacketVersion *version = reinterpret_cast<PacketVersion *>(echo);
        version->info_length = 4;
        version->major = 1;
        version->minor = 2;
        version->patch = 3;
        std::memcpy(version->info, "test", 4);
    }
    if (corrupt_response_id)
    {
        echo[1]++;
        g_keyboard_tick = g_keyboard_tick + 2;
    }
    nexus_process_buffer(slave_id, echo, len);
    return 0;
}

TEST(NexusRequest, AssignsIdAndCopiesRawResponse)
{
    reset_capture();
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;
    request[63] = 0xA5;

    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1,
                                       response, sizeof(response)));
    EXPECT_NE(0, response[1]);
    EXPECT_EQ(PACKET_CODE_GET, response[0]);
    EXPECT_EQ(PACKET_DATA_CONFIG, response[2]);
    EXPECT_EQ(0xA5, response[63]);
    EXPECT_EQ(0, g_nexus_slave_buffer[0][0]);
}

TEST(NexusRequest, RejectsResponseWithWrongId)
{
    reset_capture();
    corrupt_response_id = true;
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;

    EXPECT_EQ(1, nexus_request_timeout(0, request, sizeof(request), 2,
                                       response, sizeof(response)));
}

TEST(NexusRequest, IgnoresStaleEchoLeftInBuffer)
{
    reset_capture();
    // An echo from a previous request is still sitting in the buffer.
    uint8_t stale[NEXUS_RX_BUFFER_SIZE] = {PACKET_CODE_GET, 77, PACKET_DATA_CONFIG};
    nexus_process_buffer(0, stale, sizeof(stale));

    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;
    request[10] = 0x3C;

    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1,
                                       response, sizeof(response)));
    EXPECT_NE(77, response[1]);
    EXPECT_EQ(0x3C, response[10]);
}

TEST(NexusRequest, CopiesVersionResponse)
{
    reset_capture();
    synthesize_version_response = true;
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_VERSION;

    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1,
                                       response, sizeof(response)));
    const PacketVersion *version = reinterpret_cast<const PacketVersion *>(response);
    EXPECT_NE(0, version->id);
    EXPECT_EQ(PACKET_DATA_VERSION, version->type);
    EXPECT_EQ(4, version->info_length);
    EXPECT_EQ(1u, version->major);
    EXPECT_EQ(2u, version->minor);
    EXPECT_EQ(3u, version->patch);
    EXPECT_EQ(0, std::memcmp(version->info, "test", 4));
}

TEST(NexusRequest, EventDoesNotConsumeTransactionIdField)
{
    reset_capture();
    PacketEvent event = {};
    event.code = PACKET_CODE_EVENT;
    event.flag = PACKET_EVENT_CONFIG_CHANGED;

    ASSERT_EQ(0, nexus_send_timeout(0, reinterpret_cast<uint8_t *>(&event),
                                    sizeof(event), 1));
    ASSERT_EQ(1u, captured_packet_count);
    EXPECT_EQ(PACKET_CODE_EVENT, captured_packets[0].packet.header.code);
    EXPECT_EQ(PACKET_EVENT_CONFIG_CHANGED, captured_packets[0].packet.header.id);
}

TEST(NexusRequest, GivesUpWhenTransportKeepsFailingEvenWithoutTick)
{
    reset_capture();
    fail_send = true;
    uint8_t request[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;

    // The tick never advances here, so only the retry limit can end the call.
    EXPECT_EQ(1, nexus_request_timeout(0, request, sizeof(request), 1000, NULL, 0));
}

TEST(NexusConfigSync, InitDoesNotBlockAndWaitsForTheSlave)
{
    reset_capture();
    set_test_config(2);
    set_test_config(5);
    set_test_config(8);

    EXPECT_EQ(0u, captured_packet_count);
    nexus_poll();
    EXPECT_EQ(0u, captured_packet_count) << "nothing is sent while the slave is silent";
    EXPECT_FALSE(nexus_slave_is_online(0));

    deliver_report(0, 0, 0, 0);
    EXPECT_TRUE(nexus_slave_is_online(0));
    for (int i = 0; i < 8; i++)
    {
        nexus_poll();
    }

    ASSERT_TRUE(captured_decode_ok);
    ASSERT_EQ(3u, captured_packet_count);
    EXPECT_EQ(0u, captured_packets[0].slave_id);
    EXPECT_EQ(PACKET_CODE_SET, captured_packets[0].packet.header.code);
    EXPECT_EQ(PACKET_DATA_ADVANCED_KEY, captured_packets[0].packet.header.type);
    EXPECT_EQ(0u, captured_packets[0].packet.index);
    EXPECT_EQ(1u, captured_packets[1].packet.index);
    EXPECT_EQ(2u, captured_packets[2].packet.index);
    EXPECT_EQ(g_keyboard_advanced_keys[2].config.activation_value, captured_packets[0].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value, captured_packets[1].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[8].config.activation_value, captured_packets[2].packet.data.activation_value);
    EXPECT_NE(captured_packets[0].packet.header.id, captured_packets[1].packet.header.id);
}

TEST(NexusConfigSync, SyncSendsMappedKeyUsingSlaveLocalIndex)
{
    reset_capture();
    bring_slave_online_and_drain();
    set_test_config(5);

    EXPECT_EQ(0, nexus_sync_advanced_key_config(5));
    nexus_poll();

    ASSERT_TRUE(captured_decode_ok);
    ASSERT_EQ(1u, captured_packet_count);
    EXPECT_EQ(1u, captured_packets[0].packet.index);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.mode, captured_packets[0].packet.data.mode);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value, captured_packets[0].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.deactivation_value, captured_packets[0].packet.data.deactivation_value);
}

TEST(NexusConfigSync, SkipsKeysNotMappedToSlaves)
{
    reset_capture();
    bring_slave_online_and_drain();

    EXPECT_EQ(0, nexus_sync_advanced_key_config(4));
    EXPECT_EQ(1, nexus_sync_advanced_key_config(ADVANCED_KEY_NUM));
    nexus_poll();

    EXPECT_EQ(0u, captured_packet_count);
}

TEST(NexusConfigSync, ResendsWhenTheEchoIsLost)
{
    reset_capture();
    bring_slave_online_and_drain();
    drop_echo = true;

    nexus_sync_advanced_key_config(8);
    nexus_poll();
    ASSERT_EQ(1u, captured_packet_count);
    nexus_poll();
    EXPECT_EQ(1u, captured_packet_count) << "one request in flight at a time";

    g_keyboard_tick += kRequestTimeoutTicks;
    deliver_report(0, 0, 0, 0);   // keep the slave online
    nexus_poll();
    nexus_poll();
    ASSERT_EQ(2u, captured_packet_count);
    EXPECT_EQ(2u, captured_packets[1].packet.index);
}

TEST(NexusConfigSync, ResendsEverythingWhenTheSlaveReconnects)
{
    reset_capture();
    bring_slave_online_and_drain();

    g_keyboard_tick += kLinkTimeoutTicks + 1;
    nexus_poll();
    EXPECT_FALSE(nexus_slave_is_online(0));
    EXPECT_EQ(0u, captured_packet_count);

    deliver_report(0, 0, 0, 0);
    for (int i = 0; i < 8; i++)
    {
        nexus_poll();
    }
    EXPECT_EQ(3u, captured_packet_count);
}

TEST(NexusConfigSync, ForegroundRequestDoesNotStarveTheSyncMachine)
{
    reset_capture();
    bring_slave_online_and_drain();
    drop_echo = true;

    nexus_sync_advanced_key_config(2);
    nexus_poll();
    ASSERT_EQ(1u, captured_packet_count);
    const uint8_t in_flight_id = captured_packets[0].packet.header.id;

    // The echo of the sync request arrives while the foreground waits for its
    // own request: it must be credited to the sync machine, not lost.
    drop_echo = false;
    uint8_t late_echo[NEXUS_RX_BUFFER_SIZE] = {PACKET_CODE_SET, in_flight_id, PACKET_DATA_ADVANCED_KEY};
    nexus_process_buffer(0, late_echo, sizeof(late_echo));

    uint8_t request[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;
    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1, NULL, 0));

    nexus_sync_advanced_key_config(5);
    nexus_poll();
    ASSERT_EQ(3u, captured_packet_count) << "the sync machine moved on to the next key";
    EXPECT_EQ(1u, captured_packets[2].packet.index);
}

TEST(NexusReport, AppliesBitmapToMappedKeys)
{
    reset_capture();

    deliver_report(0, 0, 0, 0b101);
    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.state);

    deliver_report(0, 0, 0, 0b010);
    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
}

TEST(NexusReport, ReleasesKeysWhenTheSlaveGoesSilent)
{
    reset_capture();
    deliver_report(0, 0, 0, 0b111);
    nexus_process();
    ASSERT_TRUE(g_keyboard_advanced_keys[5].key.state);

    g_keyboard_tick += kLinkTimeoutTicks;
    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state) << "still within the link timeout";

    g_keyboard_tick += 1;
    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
    EXPECT_FALSE(nexus_slave_is_online(0));
}

TEST(NexusReport, RejectsFramesThatAreTooShortOrFromUnknownSlaves)
{
    reset_capture();
    PacketNexus packet;
    const volatile uint32_t bits[NEXUS_BITMAP_WORDS] = {0b111};
    nexus_report_encode(&packet, 0, 0, 0, bits, 3);

    nexus_process_buffer(0, reinterpret_cast<uint8_t *>(&packet), offsetof(PacketNexus, bits));
    nexus_process_buffer(NEXUS_SLAVE_NUM, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    nexus_process_buffer(0, NULL, sizeof(packet));
    nexus_process_buffer(0, reinterpret_cast<uint8_t *>(&packet), 0);
    nexus_process();

    EXPECT_FALSE(nexus_slave_is_online(0));
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
}

TEST(NexusReport, AcceptsAFrameEndingAtTheLastMappedBitmapByte)
{
    reset_capture();
    uint8_t frame[offsetof(PacketNexus, bits) + 1] = {};
    frame[0] = NEXUS_REPORT_FLAG | 0x7F;
    frame[offsetof(PacketNexus, bits)] = 0b101;

    nexus_process_buffer(0, frame, sizeof(frame));
    nexus_process();

    EXPECT_TRUE(nexus_slave_is_online(0));
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.state);
}

TEST(NexusReport, IgnoresExtraBitmapBytesEvenWhenCallbackExtendsTheMapping)
{
    reset_capture();
    uint16_t map[NEXUS_SLICE_LENGTH_MAX];
    for (uint16_t &id : map)
    {
        id = 0xFFFF;
    }
    map[0] = 2;
    map[NEXUS_SLICE_LENGTH_MAX - 1] = 8;
    ScopedNexusMapping mapping(map, 1);
    g_keymap_cache[2] = KEY_A;
    g_keyboard_advanced_keys[8].key.state = true;
    ScopedDownCallback callback([](Key *key) {
        if (key->id == 2)
        {
            g_nexus_slave_configs[0].length = NEXUS_SLICE_LENGTH_MAX;
            nexus_init();
        }
    });
    PacketNexus packet = {};
    packet.index = NEXUS_REPORT_FLAG | 0x7F;
    std::memset(packet.bits, 0xFF, sizeof(packet.bits));
    packet.bits[0] = 1;

    nexus_process_buffer(0, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    nexus_process();

    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
}

TEST(NexusReport, CopiesAnalogDataOfTheIndexedKeyExactly)
{
    reset_capture();
    const AnalogValue value = A_ANTI_NORM(0.37f);

    deliver_report(1, 3210, value, 0);
    EXPECT_EQ(3210, g_keyboard_advanced_keys[5].raw);
    EXPECT_EQ(3210, g_keyboard_advanced_keys[5].filtered_raw);
    EXPECT_EQ(value, g_keyboard_advanced_keys[5].value);
    EXPECT_EQ(0, g_keyboard_advanced_keys[2].raw);

    deliver_report(1, 0, ANALOG_VALUE_MAX, 0);
    EXPECT_EQ(ANALOG_VALUE_MAX, g_keyboard_advanced_keys[5].value);
}

TEST(NexusReport, OutOfRangeIndexStillAppliesTheBitmap)
{
    reset_capture();

    deliver_report(7, 999, 999, 0b100);
    nexus_process();

    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.state);
    EXPECT_EQ(0, g_keyboard_advanced_keys[2].raw);
    EXPECT_EQ(0, g_keyboard_advanced_keys[5].raw);
    EXPECT_EQ(0, g_keyboard_advanced_keys[8].raw);
}

TEST(NexusEncode, ValueRoundTripsThroughTheWire)
{
    const AnalogValue samples[] = {ANALOG_VALUE_MIN, 1, 1000, A_ANTI_NORM(0.5f), ANALOG_VALUE_MAX - 1, ANALOG_VALUE_MAX};
    for (AnalogValue sample : samples)
    {
        EXPECT_EQ(sample, nexus_value_from_wire(nexus_value_to_wire(sample)));
    }
}

TEST(NexusEncode, ReportEncoderSetsFlagAndCopiesBitmapLittleEndian)
{
    PacketNexus packet;
    const volatile uint32_t bits[NEXUS_BITMAP_WORDS] = {0x0000A5C3};

    nexus_report_encode(&packet, 5, 0x1234, 0, bits, 16);

    EXPECT_EQ(5 | NEXUS_REPORT_FLAG, packet.index);
    EXPECT_EQ(0x1234, packet.raw);
    EXPECT_EQ(0xC3, packet.bits[0]);
    EXPECT_EQ(0xA5, packet.bits[1]);

    nexus_report_encode(&packet, 0, 0, 0, bits, 11);
    EXPECT_EQ(0xC3, packet.bits[0]);
    EXPECT_EQ(0xA5 & 0x07, packet.bits[1]) << "bits beyond the key count are cleared";
}

TEST(NexusEncode, RawEncoderSplitsTheFirstSampleAroundTheFlag)
{
    uint8_t frame[8] = {0};
    const AnalogRawValue raws[] = {0x5ABC, 0x1234, 0xFFFF};

    EXPECT_EQ(6, nexus_raw_report_encode(frame, raws, 3));
    EXPECT_EQ((0x5ABC & 0x7F) | NEXUS_REPORT_FLAG, frame[0]);
    EXPECT_EQ(0x5ABC >> 7, frame[1]);
    EXPECT_EQ(0x34, frame[2]);
    EXPECT_EQ(0x12, frame[3]);
    EXPECT_EQ(0xFF, frame[4]);
    EXPECT_EQ(0xFF, frame[5]);
    EXPECT_EQ(0, nexus_raw_report_encode(frame, raws, 0));
}

// --- What the per-tick key loop must keep doing -----------------------------
// nexus_process() may skip slave keys that keyboard_key_update() could not
// affect. These pin down the cases in which the update must still run even
// though the slave's bit has not changed since the last tick.

static_assert(DEBOUNCE_PRESS > 0 && DEBOUNCE_PRESS_EAGER && DEBOUNCE_RELEASE > 0 && !DEBOUNCE_RELEASE_EAGER,
              "the debounce expectations below assume an eager press and a counted release");

TEST(NexusReport, DebounceKeepsCountingBetweenFrames)
{
    reset_capture();
    Key *key = &g_keyboard_advanced_keys[5].key;

    deliver_report(0, 0, 0, 0b010);
    nexus_process();
    ASSERT_TRUE(key->report_state) << "an eager press reports at once";

    // Released inside the press lockout: the report has to hold for the rest
    // of the lockout plus the counted release debounce, advanced tick by tick
    // although the slave sends nothing new.
    deliver_report(0, 0, 0, 0b000);
    for (int tick = 1; tick < DEBOUNCE_PRESS + DEBOUNCE_RELEASE; tick++)
    {
        g_keyboard_tick++;
        nexus_process();
        EXPECT_TRUE(key->report_state) << "tick " << tick;
    }
    g_keyboard_tick++;
    nexus_process();
    EXPECT_FALSE(key->report_state);
}

TEST(NexusReport, HeldMouseMoveKeyDispatchesEveryTick)
{
    reset_capture();
    g_keymap[0][5] = KEY_A;
    g_keymap[0][8] = KEYCODE(MOUSE_COLLECTION, MOUSE_MOVE_UP);
    layer_cache_refresh();

    deliver_report(0, 0, 0, 0b110);
    for (int tick = 0; tick < 3; tick++)
    {
        g_keyboard_report_flags.mouse = false;
        g_keyboard_report_flags.keyboard = false;
        g_keyboard_tick++;
        nexus_process();
        EXPECT_TRUE((bool)g_keyboard_report_flags.mouse) << "tick " << tick << ": movement runs while held";
        EXPECT_EQ(tick == 0, (bool)g_keyboard_report_flags.keyboard) << "tick " << tick << ": a plain key only reports its edge";
    }
}

TEST(NexusReport, LayerSwitchedBySlaveKeyAppliesToKeysProcessedAfterIt)
{
    reset_capture();
    g_keymap[0][2] = KEY_A;
    g_keymap[0][5] = LAYER(LAYER_MOMENTARY, 1);
    g_keymap[0][8] = KEY_A;
    g_keymap[1][2] = KEYCODE(MOUSE_COLLECTION, MOUSE_MOVE_UP);
    g_keymap[1][5] = KEY_TRANSPARENT;
    g_keymap[1][8] = KEYCODE(MOUSE_COLLECTION, MOUSE_MOVE_UP);
    layer_cache_refresh();

    // Slave slot order is key 2, key 5, key 8. Pressing the layer key switches
    // layers in the middle of the tick: key 8 is evaluated afterwards and
    // already runs its mouse movement (which marks the key reported), key 2
    // was evaluated before the switch and catches up on the next tick.
    deliver_report(0, 0, 0, 0b010);
    g_keyboard_tick++;
    nexus_process();
    ASSERT_EQ(1, layer_get());
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.report_state);

    g_keyboard_tick++;
    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.report_state);
}

TEST(NexusReport, UnchangedFrameRestoresExternallyChangedPhysicalState)
{
    reset_capture();
    Key *key = &g_keyboard_advanced_keys[5].key;
    deliver_report(0, 0, 0, 0);
    nexus_process();

    key->state = true;
    nexus_process();

    EXPECT_EQ(0, key->state);
    EXPECT_EQ(0, key->report_state);
    EXPECT_EQ(0, key->debounce);
}

TEST(NexusReport, UnchangedFrameDebouncesExternallyChangedReportState)
{
    reset_capture();
    Key *key = &g_keyboard_advanced_keys[5].key;
    deliver_report(0, 0, 0, 0);
    nexus_process();

    keyboard_key_set_report_state(key, true);
    for (int tick = 0; tick < DEBOUNCE_RELEASE - 1; tick++)
    {
        nexus_process();
        EXPECT_TRUE(key->report_state);
    }
    nexus_process();

    EXPECT_FALSE(key->report_state);
    EXPECT_EQ(0, key->debounce);
    EXPECT_EQ(0u, g_keyboard_bitmap[0] & (1U << 5));
}

TEST(NexusReport, NoncanonicalStateBytesStillRunTheNormalUpdate)
{
    reset_capture();
    Key *key = &g_keyboard_advanced_keys[5].key;
    deliver_report(0, 0, 0, 0b010);
    key->state = 2;
    key->report_state = 2;

    nexus_process();

    EXPECT_EQ(1, key->state);
    EXPECT_EQ(1, key->report_state);
    EXPECT_EQ(0, key->debounce);
}

TEST(NexusReport, UnchangedFrameSeesKeymapEdits)
{
    reset_capture();
    g_keymap_cache[5] = KEY_A;
    deliver_report(0, 0, 0, 0);
    nexus_process();
    ASSERT_FALSE(g_keyboard_advanced_keys[5].key.report_state);

    g_keymap_cache[5] = KEYCODE(MOUSE_COLLECTION, MOUSE_MOVE_UP);
    g_keyboard_report_flags.mouse = false;
    nexus_process();

    EXPECT_TRUE((bool)g_keyboard_report_flags.mouse);
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.report_state);
}

TEST(NexusReport, DuplicateMappingsApplyEachSlotInOrder)
{
    reset_capture();
    const uint16_t map[] = {2, 2, 0xFFFF, 5};
    ScopedNexusMapping mapping(map, 4);
    deliver_report(0, 0, 0, 0b1001);

    nexus_process();

    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.report_state);
    EXPECT_EQ(1 - DEBOUNCE_PRESS, g_keyboard_advanced_keys[2].key.debounce);
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state);

    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_EQ(3 - DEBOUNCE_PRESS, g_keyboard_advanced_keys[2].key.debounce);
}

TEST(NexusReport, CallbackEditsToLaterKeysApplyDuringTheSameTick)
{
    reset_capture();
    g_keymap_cache[2] = KEY_A;
    g_keymap_cache[8] = KEY_A;
    ScopedDownCallback callback([](Key *key) {
        if (key->id == 2)
        {
            g_keyboard_advanced_keys[8].key.state = true;
            g_keymap_cache[8] = KEYCODE(MOUSE_COLLECTION, MOUSE_MOVE_UP);
        }
    });
    deliver_report(0, 0, 0, 0b001);

    nexus_process();

    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.report_state);
    EXPECT_TRUE((bool)g_keyboard_report_flags.mouse);
}

TEST(NexusReport, FrameReceivedByCallbackTakesEffectOnTheFollowingTick)
{
    reset_capture();
    g_keymap_cache[2] = KEY_A;
    ScopedDownCallback callback([](Key *key) {
        if (key->id == 2)
        {
            deliver_report(0, 0, 0, 0);
        }
    });
    deliver_report(0, 0, 0, 0b111);

    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.state);

    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
}

TEST(NexusReport, MappingShortenedByCallbackStopsAtTheNewEnd)
{
    reset_capture();
    const uint16_t map[] = {2, 5, 8};
    ScopedNexusMapping mapping(map, 3);
    g_keymap_cache[2] = KEY_A;
    ScopedDownCallback callback([](Key *key) {
        if (key->id == 2)
        {
            g_nexus_slave_configs[0].length = 1;
            nexus_init();
        }
    });
    deliver_report(0, 0, 0, 0b111);

    nexus_process();

    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
}

TEST(NexusReport, EmptyMappingLeavesLocalKeysAlone)
{
    reset_capture();
    ScopedNexusMapping mapping(nullptr, 0);
    g_keyboard_advanced_keys[2].key.state = true;

    nexus_process();

    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
}

TEST(NexusReport, BitmapAppliesThroughTheEndOfASixteenSlotMapping)
{
    reset_capture();
    const uint16_t map[] = {16, 17, 18, 19, 20, 21, 22, 23,
                           24, 25, 26, 27, 28, 29, 30, 31};
    ScopedNexusMapping mapping(map, 16);
    deliver_report(0, 0, 0, 0x8009);

    nexus_process();

    EXPECT_TRUE(g_keyboard_advanced_keys[16].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[17].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[18].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[19].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[30].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[31].key.state);

    deliver_report(0, 0, 0, 0);
    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[16].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[19].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[31].key.state);
}

#if NEXUS_SLICE_LENGTH_MAX > 32
TEST(NexusReport, SnapshotKeepsLaterBitmapWordsWhenCallbackReceivesAnotherFrame)
{
    reset_capture();
    uint16_t map[33];
    for (uint16_t &id : map)
    {
        id = 0xFFFF;
    }
    map[0] = 2;
    map[31] = 5;
    map[32] = 8;
    ScopedNexusMapping mapping(map, 33);
    g_keymap_cache[2] = KEY_A;
    ScopedDownCallback callback([](Key *key) {
        if (key->id == 2)
        {
            deliver_report(0, 0, 0, 0, 0);
        }
    });
    deliver_report(0, 0, 0, 0x80000001U, 1);

    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[8].key.state);

    nexus_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[2].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[8].key.state);
}
#endif
