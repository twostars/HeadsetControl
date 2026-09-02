#pragma once

#include "device_utils.hpp"
#include "hid_device.hpp"

#include <array>
#include <string_view>

using namespace std::string_view_literals;

namespace headsetcontrol {

/**
 * @brief JBL Quantum 610 Wireless Gaming Headset
 *
 * Features:
 * - Battery status (0-100, no charging status)
 * - Sidetone control (0-3 levels - off/low/mid/high)
 * - RGB lighting toggle (the headset supports profiles and manual config, but this is unsupported here)
 * - Game/chat audio balance (read-only, set by the hardware dial)
 */
class JBLQuantum610Wireless : public HIDDevice {
public:
    static constexpr uint8_t CMD_GET_BATTERY  = 0x49;
    static constexpr uint8_t CMD_SET_LIGHTS   = 0x4B;
    static constexpr uint8_t CMD_SET_SIDETONE = 0x5D;
    static constexpr uint8_t CMD_GET_CHATMIX  = 0x62;

    static constexpr std::array<uint16_t, 1> SUPPORTED_PRODUCT_IDS {
        0x205c
    };

    uint16_t getVendorId() const override
    {
        return 0x0ecb; // Harman International Inc
    }

    std::vector<uint16_t> getProductIds() const override
    {
        return { SUPPORTED_PRODUCT_IDS.begin(), SUPPORTED_PRODUCT_IDS.end() };
    }

    std::string_view getDeviceName() const override
    {
        return "JBL Quantum 610 Wireless"sv;
    }

    int getCapabilities() const override
    {
        return B(CAP_LIGHTS) | B(CAP_SIDETONE) | B(CAP_CHATMIX_STATUS) | B(CAP_BATTERY_STATUS);
    }

    constexpr capability_detail getCapabilityDetail([[maybe_unused]] enum capabilities cap) const override
    {
        return { .usagepage = 0xff13, .usageid = 0x0001, .interface_id = 5 };
    }

    Result<LightsResult> setLights(hid_device* device_handle, bool on) override
    {
        std::array<uint8_t, 2> cmd { CMD_SET_LIGHTS, static_cast<uint8_t>(on ? 0x01 : 0x00) };

        if (auto result = sendFeatureReport(device_handle, cmd); !result) {
            return result.error();
        }

        return LightsResult { .enabled = on };
    }

    // Device only supports 4 levels.
    // Input 0-128 range is mapped down to the nearest of [off, low, mid, high].
    Result<SidetoneResult> setSidetone(hid_device* device_handle, uint8_t level) override
    {
        static constexpr std::array<uint8_t, 4> DEVICE_LEVELS { 0x00, 0x01, 0x02, 0x03 };
        uint8_t device_level = mapDiscrete(level, DEVICE_LEVELS);

        std::array<uint8_t, 2> cmd { CMD_SET_SIDETONE, device_level };

        if (auto result = sendFeatureReport(device_handle, cmd); !result) {
            return result.error();
        }

        return SidetoneResult {
            .current_level = level,
            .min_level     = 0,
            .max_level     = 128,
            .device_min    = 0,
            .device_max    = 3
        };
    }

    Result<ChatmixResult> getChatmix(hid_device* device_handle) override
    {
        std::array<uint8_t, 2> buffer {};
        buffer[0] = CMD_GET_CHATMIX;

        if (auto result = getFeatureReport(device_handle, buffer); !result) {
            return result.error();
        }

        uint8_t raw_value = buffer[1]; // 0x00 (full chat) - 0x10 (full game)

        int level    = map(raw_value, 0, 16, 0, 128);
        int game_pct = (level >= 64) ? 100 : map(level, 0, 64, 0, 100);
        int chat_pct = (level <= 64) ? 100 : map(level, 64, 128, 100, 0);

        return ChatmixResult {
            .level               = level,
            .game_volume_percent = game_pct,
            .chat_volume_percent = chat_pct
        };
    }

    Result<BatteryResult> getBattery(hid_device* device_handle) override
    {
        std::array<uint8_t, 2> buffer {};
        buffer[0] = CMD_GET_BATTERY;

        if (auto result = getFeatureReport(device_handle, buffer); !result) {
            return result.error();
        }

        int level = buffer[1]; // raw 0-100 percentage

        return BatteryResult {
            .level_percent = level,
            .status        = BATTERY_AVAILABLE,
            .voltage_mv    = std::nullopt
        };
    }
};

} // namespace headsetcontrol
