#include <log.h>
#include <sequans_controller.h>

#define AT_HTTPS_CONFIGURE_SECURITY_PROFILE "AT+SQNSPCFG=3,2,\"\",1,1"

void setup() {
    Log.begin(115200);

    SequansController.begin();

    Log.info("Setting up security profile for HTTPS...");
    char response[64];
    if (SequansController.writeCommand(
            AT_HTTPS_CONFIGURE_SECURITY_PROFILE, response, sizeof(response)) !=
        ResponseResult::OK) {
        Log.info("Failed to set security profile");
        return;
    }

    Log.info("Done!");
}

void loop() {}
