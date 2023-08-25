#include "security_profile.h"

#include "sequans_controller.h"

#include "log.h"

#define SECURITY_PROFILE_PREFIX_LENGTH 11

SecurityProfileClass SecurityProfile = SecurityProfileClass::instance();

bool SecurityProfileClass::profileExists(const uint8_t id) {

    char response[256]          = "";
    const ResponseResult result = SequansController.writeCommand(
        F("AT+SQNSPCFG"),
        response,
        sizeof(response));

    if (result != ResponseResult::OK) {
        Log.error(F("Failed to query security profile"));
        return false;
    }

    // Split by line feed and carriage return to retrieve each entry
    char* ptr = strtok(response, "\r\n");

    while (ptr != NULL) {

        // Skip the prefix of '+SQNSPCFG: '
        ptr += SECURITY_PROFILE_PREFIX_LENGTH;

        int security_profile_id;
        sscanf(ptr, "%d", &security_profile_id);

        if (security_profile_id == id) {
            return true;
        }

        ptr = strtok(NULL, "\r\n");
    }

    return false;
}