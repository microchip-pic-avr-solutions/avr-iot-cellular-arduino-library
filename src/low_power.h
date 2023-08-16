#ifndef LOW_POWER_H
#define LOW_POWER_H

#include <stdint.h>

/**
 * @brief Multipliers for cellular network power save mode when the cellular
 * modem is periodically sleeping.
 *
 * These values are according to 3GPP specification for timer T3412.
 */
enum class PowerSaveModePeriodMultiplier {
    TEN_MINUTES    = 0,
    ONE_HOUR       = 1,
    TEN_HOURS      = 2,
    TWO_SECONDS    = 3,
    THIRTY_SECONDS = 4,
    ONE_MINUTE     = 5,
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
    static LowPowerClass& instance(void) {
        static LowPowerClass instance;
        return instance;
    }

    /**
     * @brief Configures the low power module for power down configuration.
     */
    void configurePowerDown(void);

    /**
     * @brief Used to configure power save mode for the cellular modem. The
     * total power save time period for the cellylar modem will be
     * @p power_save_mode_period_multiplier  * @p power_save_mode_period_value.
     * This will make the cellular modem be active for some time and sleep the
     * remaining time of this time period. This repeats as long as we tell the
     * modem to save power with the #powerSave() function. This will thus save
     * quite a lot of power spent by the cellular modem.
     *
     * This cellular modem keeps track of where it is within this time period,
     * we simply have to tell it that it can save power with the #powerSave()
     * function and it will attempt to sleep in the current time period it is
     * in.
     *
     * @note The period might be different than what requested as
     * the operator has to approve this setting. Thus, the period
     * might deviate from this value. If that is so, a warning will be logged.
     *
     * @param power_save_mode_period_value Note that max value is 31.
     */
    void configurePeriodicPowerSave(
        const PowerSaveModePeriodMultiplier power_save_mode_period_multiplier,
        const uint8_t power_save_mode_period_value);

    /**
     * @brief Will attempt to put the modem in power save and then power down
     * the MCU for the time configured in configurePeriodicPowerSave(). Note
     * that this happens sequentially, first it will put the modem to sleep and
     * then the MCU. This process might take some time, so the total time both
     * of the units are asleep is will be some seconds shorter than the total
     * sleep time.
     */
    void powerSave(void);

    /**
     * @brief Will power down both CPU and cellular modem. All active
     * connections on the modem will be terminated.
     *
     * @power_down_time_seconds Seconds to remain powered down.
     */
    void powerDown(const uint32_t power_down_time_seconds);

    /**
     * @return The current voltage supplied to the board.
     */
    float getSupplyVoltage(void);
};

extern LowPowerClass LowPower;

#endif
