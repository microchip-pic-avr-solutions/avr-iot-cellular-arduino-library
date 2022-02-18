#include <Arduino.h>
#include <log.h>
#include <lte.h>

// NB: For this example it is important that this variable is volatile so the
// compiler won't optimize away the code we have in the loop function and thus
// make it so that the miss out on the event. This is due to compiler
// optimization. If something more is done during the loop function, this won't
// be a problem.
volatile bool power_save_got_abrupted = false;

void power_save_abrupted(void) { power_save_got_abrupted = true; }

String
convertAwakeMultiplierToString(const AwakeUnitMultiplier awake_multiplier) {
    switch (awake_multiplier) {
    case AwakeUnitMultiplier::TWO_SECONDS:
        return "TWO_SECONDS";
    case AwakeUnitMultiplier::ONE_MINUTE:
        return "ONE_MINUTE";
    case AwakeUnitMultiplier::SIX_MINUTES:
        return "SIX_MINUTES";
    default:
        return "UNKNOWN";
    }
}

String
convertSleepMultiplierToString(const SleepUnitMultiplier sleep_multiplier) {
    switch (sleep_multiplier) {
    case SleepUnitMultiplier::TEN_MINUTES:
        return "TEN_MINUTE";
    case SleepUnitMultiplier::ONE_HOUR:
        return "ONE_HOUR";
    case SleepUnitMultiplier::TEN_HOURS:
        return "TEN_HOURS";
    case SleepUnitMultiplier::TWO_SECONDS:
        return "TWO_SECONDS";
    case SleepUnitMultiplier::THIRTY_SECONDS:
        return "THIRTY_SECONDS";
    case SleepUnitMultiplier::ONE_MINUTE:
        return "ONE_MINUTE";
    default:
        return "UNKNOWN";
    }
}

void setup() {

    Log.begin(115200);
    Log.setLogLevel(LogLevel::INFO);

    // Set callback for when we leave power save mode
    Lte.onPowerSaveAbrupted(power_save_abrupted);

    // Start LTE modem, configure the power save configuration and wait until we
    // are connected to the operator
    //
    // Here we say that we want to sleep for 30 seconds * 3 = 90 seconds = 1.5
    // minutes and that we want our awake period to be at 2 seconds * 8 = 16
    // seconds
    Lte.begin(true,
              PowerSaveConfiguration{SleepUnitMultiplier::THIRTY_SECONDS,
                                     3,
                                     AwakeUnitMultiplier::TWO_SECONDS,
                                     8});

    while (!Lte.isConnected()) {
        Log.info("Waiting for connection...\r\n");
        delay(2000);
    }

    Log.info("Connected to operator!\r\n");

    // After we are connected, we can check which schedule for the network
    // contact the operator gave us, which may deviate from what we requested
    //
    // In the end, the operator has the final say in this, so we have to use
    // what we are given
    //
    // The following is just for debug purposes
    PowerSaveConfiguration current_power_save_cfg =
        Lte.getCurrentPowerSaveConfiguration();

    String sleep_multiplier_string =
        convertSleepMultiplierToString(current_power_save_cfg.sleep_multiplier);
    String awake_multiplier_string =
        convertAwakeMultiplierToString(current_power_save_cfg.awake_multiplier);

    Log.infof("Power saving configuration given by operator. Sleep multiplier: "
              "%s, sleep value: %d, awake multiplier: %s, awake value: %d\r\n",
              sleep_multiplier_string.c_str(),
              current_power_save_cfg.sleep_value,
              awake_multiplier_string.c_str(),
              current_power_save_cfg.awake_value);

    // Now we attempt to enter power save mode. We do this in a loop as it might
    // fail if the LTE modem is currently busy doing work.
    while (!Lte.attemptToEnterPowerSaveMode()) {
        Log.info("Failed to put LTE modem in sleep, retrying...\r\n");
    }

    Log.info("LTE modem is sleeping!\r\n");
}

void loop() {

    // When we got abrupted from power save mode, which will happen when the
    // modem goes out of sleep in the period we have specified or receives some
    // message, we have to manually put it back to sleep
    if (power_save_got_abrupted) {
        Log.info("Power save abrupted/sleep finished, doing work...\r\n");

        // Do work, check messages, report status etc...

        Log.info("Attempting to put LTE modem back into sleep...\r\n");

        // The modem might be active doing work and this request for power save
        // mode might fail before the timeout, so we retry until we succeed
        //
        // An extended timeout can be given by
        // Lte.attemptToEnterPowerSaveMode(waiting_time_ms). The default is
        // 60000 ms = 60 seconds
        while (!Lte.attemptToEnterPowerSaveMode()) {}

        Log.info("Success!\r\n");
        power_save_got_abrupted = false;
    }
}
