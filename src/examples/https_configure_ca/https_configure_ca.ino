#include <log.h>
#include <sequans_controller.h>

#define AT_HTTPS_CONFIGURE_SECURITY_PROFILE "AT+SQNSPCFG=3,2,\"\",1,1"

void setup() {
    Log.begin(115200);

    SequansController.begin();

    // Allow time for boot
    delay(500);

    Log.info("Setting up security profile for HTTPS...");
    if (!SequansController.retryCommand(AT_HTTPS_CONFIGURE_SECURITY_PROFILE)) {
        Log.info("Failed to set security profile");
        return;
    }
    Log.info("Done!");
}

void loop() {}
