/**
 * @brief Higher level interface for interacting with the LTE module.
 */

#ifndef LTE_H
#define LTE_H

#include <stdint.h>

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

class LteClass {

  private:
    /**
     * @brief Constructor is hidden to enforce a single instance of this class
     * through a singleton.
     */
    LteClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static LteClass &instance(void) {
        static LteClass instance;
        return instance;
    }

    /**
     * @brief Initializes the LTE module and its controller interface. Starts
     * searching for operator.
     */
    void begin(void);

    /**
     * @brief Disables the interface with the LTE module. Disconnects from
     * operator.
     */
    void end(void);

    /**
     * @brief Registers callback functions for when the module is connected to
     * the operator and disconnected from the operator.
     */
    void onConnectionStatusChange(void (*connect_callback)(void),
                                  void (*disconnect_callback)(void));

    bool isConnected(void);

    /**
     * @brief Will set up configuration to put the LTE modem in a periodic
     * sleep and thus save power.
     *
     * @note This method has to be called before begin().
     *
     * When the modem is sleeping it will sleep for @p sleep_multiplier *
     * @p sleep_value time units, then be awake for @p awake_multiplier * @p
     * awake_value time units whilst checking the network connecting and
     * receiving messages before going back to sleep again. This repeats
     * indefinitely until stopped or abrupted.
     *
     * @param sleep_multiplier Value to multiply @p sleep_value with.
     * @param sleep_value How long the LTE modem will sleep during a periodic
     * sleep schedule, multiplied with @p sleep_multiplier. @note Max value
     * is 31.
     *
     * @param awake_multiplier Value to multiply @p awake_value with.
     * @param awake_value How long the LTE modem will be awake during a periodic
     * sleep schedule, multiplied with @p awake_multiplier. @note Max value
     * is 31.
     *
     * @return true If configuration was set successfully.
     */
    bool configurePowerSaveMode(const SleepUnitMultiplier sleep_multiplier,
                                const uint8_t sleep_value,
                                const AwakeUnitMultiplier awake_multiplier,
                                const uint8_t awake_value);

    /**
     * @brief Will attempt to put the LTE modem in power save mode.
     *
     * @note Will wait for @p waiting_time to see if the modem gets to low power
     * mode.
     *
     * If no previous configuration is set, the modem will sleep with the
     * default configuration of 10 minutes with sleep and 1 minute awake
     * receiving messages before going back to sleep.
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
    bool attemptToEnterPowerSaveMode(const uint32_t waiting_time_ms = 30000);

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

extern LteClass Lte;

#endif
