#include <log.h>
#include <sequans_controller.h>

#define AT_HTTPS_CONFIGURE_SECURITY_PROFILE "AT+SQNSPCFG=3,2,\"\",1,1"

void setup() {
    Log.begin(115200);

    SequansController.begin();

    Log.info("Setting up security profile for HTTPS...");

    SequansController.writeBytes((uint8_t*)AT_HTTPS_CONFIGURE_SECURITY_PROFILE,
                                 strlen(AT_HTTPS_CONFIGURE_SECURITY_PROFILE),
                                 true);

    if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
        Log.infof("Failed to set security profile\r\n");
        return;
    }

    Log.info("Done!");
}

void loop() {}
