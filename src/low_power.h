#ifndef LOW_POWER_H
#define LOW_POWER_H

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Multipliers for LTE network sleep period when periodically sleeping.
 *
 * These values are according to 3GPP specification for timer T3412.
 */
enum class SleepUnitMultiplier {
    TEN_MINUTES = 0,
    ONE_HOUR = 1,
    TEN_HOURS = 2,
    TWO_SECONDS = 3,
    THIRTY_SECONDS = 4,
    ONE_MINUTE = 5,
};

/**
 * @brief Multipliers for LTE network active period when periodically sleeping.
 *
 * These values are according to 3GPP specification for timer T3324.
 */
enum class AwakeUnitMultiplier {
    TWO_SECONDS = 0,
    ONE_MINUTE = 1,
    SIX_MINUTES = 2,
};

/**
 * @brief Power save configuration structure where the total time sleeping is @p
 * sleep_multiplier * @p sleep_value and the total time awake is @p
 * awake_multiplier * @p awake_value.
 *
 * @note The max values for @p sleep_value and @p awake_value is 2^5 - 1 = 31.
 */
struct PowerSaveConfiguration {
    SleepUnitMultiplier sleep_multiplier;
    uint8_t sleep_value;
    AwakeUnitMultiplier awake_multiplier;
    uint8_t awake_value;
};

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
     * @brief Initializes the LTE module and its controller interface. Starts
     * searching for operator. Can be used to configure power save
     * configuration.
     *
     *
     * Power saving configuration:
     *
     * @param power_save_configuration The configuration for the sleep schedule.
     * Will sleep #PowerSaveConfiguration.sleep_multiplier *
     * #PowerSaveConfiguration.sleep_value before awaking for
     * #PowerSaveConfiguration.awake_multiplier *
     * PowerSaveConfiguration.awake_value.
     *
     *
     * @note The max value for #PowerSaveConfiguration.sleep_value and
     * #PowerSaveConfiguration.awake_value is 31.
     *
     * @note The values requested for power saving might be rejected by the
     * operator and modified, thus this has to be checked in with
     * #getCurrentPowerSaveConfiguration().
     *
     * @return True if configuration was set successfully.
     */
    bool begin(const PowerSaveConfiguration power_save_configuration =
                   {SleepUnitMultiplier::THIRTY_SECONDS,
                    6,
                    AwakeUnitMultiplier::TWO_SECONDS,
                    30},
               void (*on_sleep_finished)(void) = NULL);

    /**
     * @brief Will attempt to put the LTE modem in power save mode. If this
     * method is called before begin(), the modem will be put in low power mode
     * until brought back by endPowerSaveMode(), no automatic periodicity will
     * happen as is the case when begin() is called first and the modem is
     * connected to the network.
     *
     * @note Will wait for @p waiting_time to see if the modem gets to low power
     * mode. If no previous configuration is set, the modem will not be able to
     * go into power save mode.
     *
     * @note The power save mode can be abrupted if a new message arrives from
     * the network (for example a MQTT message). Such messages have to be
     * handled before the LTE modem can be put back in into power save mode.
     *
     * @param waiting_time_ms How long to wait for the modem to get into low
     * power mode before giving up (in milliseconds).
     *
     * @return true if the LTE modem was put in power save mode.
     */
    bool attemptToEnterPowerSaveMode(const uint32_t waiting_time_ms = 60000);

    /**
     * @return The configuration set by the operator, which may deviate from the
     * configuration set by the user if the operator don't support such a
     * configuration. If failed to retrieve the configuration, it will be blank.
     */
    PowerSaveConfiguration getCurrentPowerSaveConfiguration(void);

    /**
     * @brief Stops the power save mode.
     */
    void endPowerSaveMode(void);

    /**
     * @brief Registers a callback which is called when the LTE modem is
     * abrupted from power save mode. This can happen if a message arrived or
     * something else happened.
     *
     * @param callback The callback which will be called when power save mode is
     * abrupted from some external event.
     */
    void onPowerSaveAbrupted(void (*power_save_abrupted_callback)(void));

    bool isInPowerSaveMode(void);
};

extern LowPowerClass LowPower;

#endif
