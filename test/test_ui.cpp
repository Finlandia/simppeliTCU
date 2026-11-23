// Unit tests for simppeliTCU/ui.h (sendMainPage)
// The page is rendered into the host shim's WebServer; its response is read back through the
// test-only accessors (SIMPPELITCU_UNIT_TEST) and checked for the expected HTML fragments.
#include <gtest/gtest.h>

#include <string>

#include "ui.h"

namespace {

// All inputs of sendMainPage, defaulting to "nothing received from the car yet"
struct PageState {
    float soc = -1.0f;
    float cabinTemp = -99.0f;
    bool isCharging = false;
    ChargerState chargerState = ChargerState::IDLE;
    bool isHvacOn = false;
    bool sequenceActive = false;
    bool lockingEnabled = false;
    int8_t lockState = -1;
    float setpoint = -1.0f;
    float fanSpeed = -1.0f;
    bool heating = false;
    bool cooling = false;
    VentilationMode ventilation = VentilationMode::UNKNOWN;
    LeafGpsData gps{};
    bool tracking = false;
};

class Ui : public ::testing::Test {
protected:
    std::string render(const PageState& s) {
        sendMainPage(server, s.soc, s.cabinTemp, s.isCharging, s.chargerState, s.isHvacOn,
                     s.sequenceActive, s.lockingEnabled, s.lockState, s.setpoint, s.fanSpeed,
                     s.heating, s.cooling, s.ventilation, s.gps, s.tracking);
        return server.responseBody();
    }

    static bool contains(const std::string& page, const std::string& text) {
        return page.find(text) != std::string::npos;
    }

    WebServer server;
    PageState state;
};

#define EXPECT_CONTAINS(page, text) EXPECT_TRUE(contains(page, text)) << "missing: " << (text)
#define EXPECT_NOT_CONTAINS(page, text) EXPECT_FALSE(contains(page, text)) << "unexpected: " << (text)

// ---------------------------------------------------------------- Page frame

TEST_F(Ui, SendsCompleteHtmlDocument) {
    const std::string page = render(state);

    EXPECT_EQ(server.responseStatus(), 200);
    EXPECT_EQ(server.responseContentType(), "text/html");
    EXPECT_EQ(page.rfind("<html>", 0), 0u);
    EXPECT_EQ(page.substr(page.size() - 15), "</body></html>\n");
    EXPECT_CONTAINS(page, "<h1>Leaf Remote Control</h1>");
    EXPECT_CONTAINS(page, "simppeliTCU " FW_FULL_VERSION);
}

TEST_F(Ui, AlwaysShowsCommandButtons) {
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "href='/refresh'");
    EXPECT_CONTAINS(page, "action='/hvac_on'");
    EXPECT_CONTAINS(page, "href='/hvac_off'");
    EXPECT_CONTAINS(page, "href='/charge_on'");
}

// ---------------------------------------------------------------- Car status box

TEST_F(Ui, ShowsPlaceholdersBeforeDataArrives) {
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "<b>Battery:</b> Waiting...");
    EXPECT_CONTAINS(page, "<b>Cabin Temp:</b> Waiting...");
    EXPECT_CONTAINS(page, "<b>Charger:</b> Idle");
    EXPECT_CONTAINS(page, "<b>Doors:</b> Waiting...");
    EXPECT_CONTAINS(page, "<b>Location:</b> Disabled (tracking off)");
}

TEST_F(Ui, ShowsBatteryWithOneDecimal) {
    state.soc = 80.46f;
    EXPECT_CONTAINS(render(state), "<b>Battery:</b> 80.5 %");

    state.soc = 0.0f;  // 0 % is a valid reading, not "Waiting"
    EXPECT_CONTAINS(render(state), "<b>Battery:</b> 0.0 %");
}

TEST_F(Ui, ShowsCabinTempOnlyInPlausibleRange) {
    state.cabinTemp = 21.0f;
    EXPECT_CONTAINS(render(state), "<b>Cabin Temp:</b> 21.0 &deg;C");

    state.cabinTemp = -29.5f;
    EXPECT_CONTAINS(render(state), "<b>Cabin Temp:</b> -29.5 &deg;C");

    state.cabinTemp = -30.0f;  // range is exclusive on both ends
    EXPECT_CONTAINS(render(state), "<b>Cabin Temp:</b> Waiting...");

    state.cabinTemp = 80.0f;
    EXPECT_CONTAINS(render(state), "<b>Cabin Temp:</b> Waiting...");
}

TEST_F(Ui, ShowsEveryChargerState) {
    const std::pair<ChargerState, const char*> cases[] = {
        {ChargerState::IDLE, "Idle"},
        {ChargerState::CHARGING, "Charging"},
        {ChargerState::FINISHED, "Finished"},
        {ChargerState::INTERRUPTED, "Interrupted"},
        {ChargerState::WAITING, "Waiting"},
    };
    for (const auto& c : cases) {
        state.chargerState = c.first;
        EXPECT_CONTAINS(render(state), std::string("<b>Charger:</b> ") + c.second + "</p>");
    }
}

TEST_F(Ui, ShowsLockState) {
    state.lockState = 1;
    EXPECT_CONTAINS(render(state), "<b>Doors:</b> Locked");

    state.lockState = 0;
    EXPECT_CONTAINS(render(state), "<b>Doors:</b> Unlocked");
}

TEST_F(Ui, ShowsLocationLinkWhenTracking) {
    state.tracking = true;
    state.gps.latitude = 60.169667;
    state.gps.longitude = 24.95225;
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "href='https://maps.google.com/?q=60.169667,24.952250'");
    EXPECT_CONTAINS(page, "rel='noopener noreferrer'>60.169667, 24.952250</a>");
}

TEST_F(Ui, ShowsLocationWaitingWithoutFix) {
    state.tracking = true;  // 0,0 means no GPS data received yet
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "<b>Location:</b> Waiting...");
    EXPECT_NOT_CONTAINS(page, "maps.google.com");
}

TEST_F(Ui, HidesLocationWhenTrackingDisabled) {
    state.gps.latitude = 60.169667;
    state.gps.longitude = 24.95225;
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "Disabled (tracking off)");
    EXPECT_NOT_CONTAINS(page, "60.169667");
}

// ---------------------------------------------------------------- Running command

TEST_F(Ui, AutoRefreshesWhileSequenceActive) {
    state.sequenceActive = true;
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "<meta http-equiv='refresh' content='2'>");
    EXPECT_CONTAINS(page, "Processing command...");
}

TEST_F(Ui, NoAutoRefreshWhenIdle) {
    const std::string page = render(state);

    EXPECT_NOT_CONTAINS(page, "http-equiv='refresh'");
    EXPECT_NOT_CONTAINS(page, "Processing command...");
}

// ---------------------------------------------------------------- HVAC and charging indicators

TEST_F(Ui, HidesHvacDetailsWhenOff) {
    state.heating = true;
    state.setpoint = 22.0f;
    state.fanSpeed = 50.0f;
    const std::string page = render(state);

    EXPECT_NOT_CONTAINS(page, "HVAC ON");
    EXPECT_NOT_CONTAINS(page, "Heating Mode");
    EXPECT_NOT_CONTAINS(page, "Fan Speed");
    EXPECT_NOT_CONTAINS(page, "Ventilation:");
}

TEST_F(Ui, ShowsHvacDetailsWhenOn) {
    state.isHvacOn = true;
    state.heating = true;
    state.setpoint = 22.5f;
    state.fanSpeed = 57.14f;
    state.ventilation = VentilationMode::FACE;
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "HVAC ON");
    EXPECT_CONTAINS(page, "Heating Mode");
    EXPECT_NOT_CONTAINS(page, "Cooling Mode");
    EXPECT_CONTAINS(page, "Setpoint: 22.5 &deg;C");
    EXPECT_CONTAINS(page, "Fan Speed: 57%");
    EXPECT_CONTAINS(page, "<b>Ventilation:</b> Face</p>");
}

TEST_F(Ui, HidesUnknownSetpointAndFanSpeed) {
    state.isHvacOn = true;
    state.cooling = true;
    const std::string page = render(state);

    EXPECT_CONTAINS(page, "Cooling Mode");
    EXPECT_NOT_CONTAINS(page, "Setpoint:");
    EXPECT_NOT_CONTAINS(page, "Fan Speed:");
    EXPECT_CONTAINS(page, "<b>Ventilation:</b> Unknown</p>");
}

TEST_F(Ui, ShowsEveryVentilationMode) {
    state.isHvacOn = true;
    const std::pair<VentilationMode, const char*> cases[] = {
        {VentilationMode::OFF, "Off"},
        {VentilationMode::FACE, "Face"},
        {VentilationMode::FACE_FEET, "Face & Feet"},
        {VentilationMode::FEET, "Feet"},
        {VentilationMode::WINDSCREEN_FEET, "Windscreen & Feet"},
        {VentilationMode::WINDSCREEN, "Windscreen"},
        {VentilationMode::UNKNOWN, "Unknown"},
    };
    for (const auto& c : cases) {
        state.ventilation = c.first;
        EXPECT_CONTAINS(render(state), std::string("<b>Ventilation:</b> ") + c.second + "</p>");
    }
}

TEST_F(Ui, ShowsChargingIndicator) {
    EXPECT_NOT_CONTAINS(render(state), "CHARGING ON");

    state.isCharging = true;
    EXPECT_CONTAINS(render(state), "CHARGING ON");
}

// ---------------------------------------------------------------- Controls

TEST_F(Ui, SetpointFieldDefaultsTo20) {
    EXPECT_CONTAINS(render(state), "id='setpoint' value='20.0'");

    state.setpoint = 0.0f;  // car reports 0 during remote climate
    EXPECT_CONTAINS(render(state), "id='setpoint' value='20.0'");
}

TEST_F(Ui, SetpointFieldUsesCurrentSetpoint) {
    state.setpoint = 23.5f;
    EXPECT_CONTAINS(render(state), "id='setpoint' value='23.5'");
}

TEST_F(Ui, LockButtonsOnlyWhenLockingEnabled) {
    std::string page = render(state);
    EXPECT_NOT_CONTAINS(page, "href='/unlock'");
    EXPECT_NOT_CONTAINS(page, "href='/lock'");

    state.lockingEnabled = true;
    page = render(state);
    EXPECT_CONTAINS(page, "href='/unlock'");
    EXPECT_CONTAINS(page, "href='/lock'");
}

TEST_F(Ui, RenderingTwiceProducesSamePage) {
    // send() starts a new response, so a reused server must not accumulate old content
    const std::string first = render(state);
    EXPECT_EQ(render(state), first);
}

}  // namespace
