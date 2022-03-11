#ifndef LOW_POWER_H
#define LOW_POWER_H

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Multipliers for LTE network sleep period when periodically sleeping.
 *
 * These values are according to 3GPP specification for timer T3412.
 */
enum class SleepMultiplier {
    TEN_MINUTES = 0,
    ONE_HOUR = 1,
    TEN_HOURS = 2,
    TWO_SECONDS = 3,
    THIRTY_SECONDS = 4,
    ONE_MINUTE = 5,
};

enum class SleepStatusCode {
    // Invoked if the sleep time retrieved from the operator wasn't valid
    INVALID_SLEEP_TIME = 4,

    // Invoked if it took so long to put the modem in sleep that it wasn't time
    // left for the CPU to sleep. The sleep time should be considered to be
    // increased.
    TIMEOUT = 3,

    // The modem went out of sleep before the total time, which may happen if
    // e.g. the interval of needing to send MQTT heartbeat is lower than the
    // sleep time.
    AWOKEN_BY_MODEM_PREMATURELY = 2,

    // If some unknown external interrupt caused the CPU to wake up
    AWOKEN_BY_EXTERNAL_EVENT = 1,

    OK = 0,
};

enum class SleepMode { REGULAR, DEEP };

class LowPowerClass {

  private:
    /**
     * @brief Hide constructor in order to enforce a single instance of the
     * class.
     */
    LowPowerClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static LowPowerClass &instance(void) {
        static LowPowerClass instance;
        return instance;
    }

    /**
     * @brief Used to configure power save mode. The total sleep period will be
     * @p sleep_multiplier * @p sleep_value.
     *
     * @note The total sleep period might be different than what requested as
     * the operator has to approve this setting. Thus, the total time asleep
     * might deviate from this value.
     *
     * @param sleep_value Note that max value is 31.
     *
     * @return True if configuration was set successfully.
     */
    bool begin(const SleepMultiplier sleep_multiplier,
               const uint8_t sleep_value);

    /**
     * @brief Will attempt to put the modem in sleep and then the MCU for the
     * time configured in begin(). Note that this happens sequentially, first it
     * will put the modem to sleep and then the MCU. This process might take
     * some time, so the total time both of the units are asleep is highly
     * likely to be some seconds shorter than the total sleep time.
     *
     * @param sleep_modem:
     * - REGULAR: Modem in sleep, CPU in deep sleep
     * - DEEP: Modem powered off, CPU in deep sleep
     *
     * @return Status code:
     * - INVALID_SLEEP_TIME: if the sleep time configured in begin() wasn't
     * valid or something else failed. Consider modifying the sleep time.
     *
     * - TIMEOUT: Happens when the remining time after putting the modem to
     * sleep left there being no time for the CPU to sleep. Not necessarily a
     * problem, but the CPU won't get any sleep time. This can be alleviated by
     * increasing the sleep time.
     *
     * - AWOKEN_BY_MODEM_PREMATURELY: Can happen if for example the MQTT keep
     * alive interval is shorter than the sleep time configured. Then the modem
     * has to wake up in order to message the MQTT broker before the total sleep
     * time has elapsed. This is not necessarily a problem, as the modem and CPU
     * can be put to sleep after the matter causing the wake up has been taken
     * care of.
     *
     * - AWOKEN_BY_EXTERNAL_EVENT: Awoken by some external unknown interrupt.
     *
     * - OK: Sleep went fine.
     */
    SleepStatusCode sleep(const SleepMode sleep_mode);
};

extern LowPowerClass LowPower;

#endif
