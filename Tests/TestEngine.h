#pragma once
#include "../Source/SpectralEngine.h"
// Convenience for direct, single-threaded DSP tests whose settings vary per render.
// Production processor tests use SpectralEngine itself, without this adapter.
class TestEngine : public SpectralEngine
{
public:
    void process(const float* in, float* wet, float* dry, int count, const Settings& settings)
    {
        allocateFor(settings.chain);
        SpectralEngine::process(in, wet, dry, count, settings);
    }
};
