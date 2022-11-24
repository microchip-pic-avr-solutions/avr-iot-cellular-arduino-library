#ifndef SECURITY_PROFILE
#define SECURITY_PROFILE

#include <stdint.h>

class SecurityProfileClass {

  private:
    SecurityProfileClass(){};

  public:
    /**
     * @brief Singleton instance.
     */
    static SecurityProfileClass& instance(void) {
        static SecurityProfileClass instance;
        return instance;
    }

    /**
     * @brief Probes the modem for whether a certain security profile exists.
     *
     * @param id The security profile identifier.
     *
     * @return true if it exists.
     */
    bool profileExists(const uint8_t id);
};

extern SecurityProfileClass SecurityProfile;

#endif