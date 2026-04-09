#include "OddEvenStrategy.h"
#include "../HarmonicGenerator.h"

void OddEvenStrategy::resolve(std::vector<Voice>& voices)
{
    for (auto& v : voices)
    {
        if (!v.active) continue;
        if (v.voiceIndex == 0)
        {
            // voice 0 keeps odd harmonics only (H3, H5, H7 = indices 1, 3, 5)
            // zero out even harmonics (H2, H4, H6, H8 = indices 0, 2, 4, 6)
            v.amps[0] = 0.0f; v.amps[2] = 0.0f; v.amps[4] = 0.0f; v.amps[6] = 0.0f;
        }
        else if (v.voiceIndex == 1)
        {
            // voice 1 keeps even harmonics only (H2, H4, H6, H8 = indices 0, 2, 4, 6)
            // zero out odd harmonics (H3, H5, H7 = indices 1, 3, 5)
            v.amps[1] = 0.0f; v.amps[3] = 0.0f; v.amps[5] = 0.0f;
        }
    }
}
