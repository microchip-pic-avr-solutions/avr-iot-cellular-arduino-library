/**
 * @file types.h

 * @brief We need to place this in a separate file due to how Arduino compiles.
 Placing this enum in the .ino file ends up in compilation errors.
 */

#ifndef TYPES_H
#define TYPES_H

enum class OperatingMode { LteM = 1, NbIoT, Unknown };

#endif