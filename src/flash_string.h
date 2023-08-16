#ifndef FLASH_STRING_H
#define FLASH_STRING_H

/**
 * @brief Helper macro to convert from PROGMEM strings to __FlashStringHelper.
 */
#define FV(s) ((__FlashStringHelper*)(s))

#endif