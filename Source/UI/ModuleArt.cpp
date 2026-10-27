#include "ModuleArt.h"
#include "Theme.h"

using namespace StripTheme;
using ModuleType = SpectralEngine::ModuleType;

const std::vector<const char*>& artParameters(int type)
{
    static const std::vector<const char*> none,
        freeze { "freezeModule", "freeze" },
        gate { "gate", "invert", "threshold", "toneTilt", "gateAttack", "gateRelease" },
        delay { "delay", "delayMs", "spread", "feedback", "damping", "delayMix" },
        blur { "blur", "blurAmount", "blurWidth", "blurMix" },
        tremolo { "tremolo", "tremoloRate", "tremoloDepth", "tremoloShape" },
        shimmer { "reverb", "reverbDecay", "reverbSize", "reverbShimmer", "reverbTone", "reverbMix" };
    switch (type)
    {
        case ModuleType::freeze:  return freeze;
        case ModuleType::gate:    return gate;
        case ModuleType::delay:   return delay;
        case ModuleType::blur:    return blur;
        case ModuleType::tremolo: return tremolo;
        case ModuleType::shimmer: return shimmer;
        default:                  return none;
    }
}

bool artIsAnimated(int type, const ModuleArtValues& values)
{
    if (type == ModuleType::freeze)  return values.raw("freezeModule") > 0.5f && values.raw("freeze") < 0.5f;
    if (type == ModuleType::tremolo) return values.raw("tremolo") > 0.5f;
    return false;
}

// The illustrations follow the module's parameters: they are drawings of what the settings do,
// not measurements of the audio.
void drawModuleArt(juce::Graphics& g, int type, juce::Rectangle<float> r, const ModuleArtValues& values)
{
    using juce::MathConstants;
    const juce::PathStrokeType thin(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                               thick(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float x0 = r.getX(), y0 = r.getY(), w = r.getWidth(), h = r.getHeight();
    const auto& ids = artParameters(type);
    const bool enabled = ids.empty() || values.raw(ids.front()) > 0.5f;
    // A switched-off module keeps its drawing, faded.
    const float fade = enabled ? 1.0f : 0.35f;
    const auto white = text.withMultipliedAlpha(fade), blueLine = blueLight.withAlpha(0.85f*fade);
    const double seconds = values.seconds;
    switch (type)
    {
    case ModuleType::freeze: // Freeze: live partials flow; once captured they stop and the brackets close in.
    {
        const bool captured = values.raw("freeze") > 0.5f;
        const float phase = values.freezePhase;
        for (int k = 4; k >= 0; --k)
        {
            juce::Path p;
            const float cy = y0 + h*(0.3f + 0.1f*static_cast<float>(k)), amp = h*(0.05f+0.025f*static_cast<float>(k));
            for (int i = 0; i <= 80; ++i)
            {
                const float t = static_cast<float>(i)/80.0f, px = x0 + w*(0.12f+0.76f*t);
                const float py = cy - amp*std::sin(t*MathConstants<float>::twoPi*3.0f + static_cast<float>(k)*0.6f + phase*(1.0f+0.25f*static_cast<float>(k)));
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            g.setColour(k == 2 ? white : blueLine.withMultipliedAlpha(1.0f - 0.12f*static_cast<float>(std::abs(k-2))));
            g.strokePath(p, k == 2 ? thick : thin);
        }
        const float inset = captured ? 0.08f : 0.0f, b = std::min(w, h)*(captured ? 0.12f : 0.08f);
        g.setColour(captured ? blueLight.withAlpha(fade) : blueLight.withAlpha(0.35f*fade));
        for (auto c : {juce::Point<float>(x0+w*(0.04f+inset), y0+h*(0.08f+inset)), juce::Point<float>(x0+w*(0.96f-inset), y0+h*(0.08f+inset)),
                       juce::Point<float>(x0+w*(0.04f+inset), y0+h*(0.92f-inset)), juce::Point<float>(x0+w*(0.96f-inset), y0+h*(0.92f-inset))})
        {
            const float sx = c.x < r.getCentreX() ? 1.0f : -1.0f, sy = c.y < r.getCentreY() ? 1.0f : -1.0f;
            juce::Path p;
            p.startNewSubPath(c.x, c.y+sy*b);
            p.lineTo(c.x, c.y);
            p.lineTo(c.x+sx*b, c.y);
            g.strokePath(p, captured ? thick : thin);
        }
        break;
    }
    case ModuleType::gate: // Gate / tilt: tilted spectrum, threshold line, muted bins and the attack/release shape.
    {
        const bool gateOn = values.raw("gate") > 0.5f, invert = values.raw("invert") > 0.5f;
        const float threshold = values.raw("threshold"), tilt = values.raw("toneTilt");
        const float bottom = y0 + h*0.97f, top = y0 + h*0.22f;
        const auto mapDb = [&](float db) { return bottom - (bottom-top)*juce::jlimit(0.0f, 1.0f, (db+90.0f)/90.0f); };
        const int bars = 30;
        for (int i = 0; i < bars; ++i)
        {
            const float t = static_cast<float>(i)/(bars-1), px = x0 + w*(0.04f+0.92f*t);
            const float octaves = std::log2(20.0f*std::pow(1000.0f, t)/1000.0f);
            const float db = juce::jlimit(-90.0f, 0.0f, -30.0f - 4.0f*octaves + 14.0f*std::sin(t*19.0f)*std::cos(t*7.0f) + juce::jlimit(-18.0f, 18.0f, tilt*octaves));
            const bool below = db < threshold, muted = gateOn && (invert ? !below : below);
            const float y = muted ? mapDb(db - 24.0f) : mapDb(db);
            g.setColour(muted ? blueLine.withMultipliedAlpha(0.55f) : white);
            g.drawLine(px, bottom, px, std::min(bottom-1.0f, y), muted ? 1.2f : 1.6f);
        }
        juce::Path dashed, line;
        const float ty = mapDb(threshold);
        line.startNewSubPath(x0, ty); line.lineTo(x0+w, ty);
        const float dashes[] = {4.0f, 3.0f};
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, line, dashes, 2);
        g.setColour(blueLight.withAlpha((gateOn ? 1.0f : 0.35f)*fade));
        g.fillPath(dashed);
        // Attack / release envelope, log-scaled like the sliders.
        const float attack = values.normalised("gateAttack"), release = values.normalised("gateRelease");
        const auto env = juce::Rectangle<float>(x0+w*0.52f, y0, w*0.48f, h*0.14f);
        const float rise = env.getWidth()*(0.04f+0.3f*attack), fall = env.getWidth()*(0.04f+0.5f*release);
        juce::Path e;
        e.startNewSubPath(env.getX(), env.getBottom());
        e.lineTo(env.getX()+rise, env.getY());
        e.lineTo(env.getRight()-fall, env.getY()); e.quadraticTo(env.getRight()-fall*0.6f, env.getBottom(), env.getRight(), env.getBottom());
        g.setColour(white.withMultipliedAlpha(gateOn ? 0.9f : 0.4f));
        g.strokePath(e, thin);
        break;
    }
    case ModuleType::delay: // Delay: echo spacing from Time, decay from Feedback and Damping, skew from Dispersion.
    {
        const float time = values.normalised("delayMs"), spread = values.raw("spread");
        const float feedback = values.raw("feedback"), damping = values.raw("damping"), mix = values.raw("delayMix");
        const float base = y0 + h*0.92f, start = x0 + w*0.12f, dx = w*(0.05f + 0.34f*time), sigma = w*0.035f;
        g.setColour(gridMajor.withMultipliedAlpha(fade));
        g.drawLine(x0, base, x0+w, base, 1.0f);
        for (int k = 12; k >= 0; --k)
        {
            float height = h*0.8f;
            if (k > 0) height *= mix * std::pow(feedback, static_cast<float>(k-1)) * std::pow(1.0f - 0.45f*damping, static_cast<float>(k));
            const float centre = start + dx*static_cast<float>(k);
            if (height < 2.0f || centre - 3*sigma > x0+w) continue;
            // Dispersion pulls lows and highs apart: the echo gets wider and leans one way.
            const float stretch = 1.0f + 0.35f*damping*static_cast<float>(k) + 0.25f*std::abs(spread)*static_cast<float>(std::min(k, 6));
            const float lean = juce::jlimit(-0.7f, 0.7f, 0.12f*spread*static_cast<float>(std::min(k, 6)));
            const float left = sigma*stretch*(1.0f - lean), right = sigma*stretch*(1.0f + lean);
            juce::Path p;
            for (int i = 0; i <= 60; ++i)
            {
                const float u = static_cast<float>(i)/60.0f*2.0f - 1.0f;
                const float px = centre + u*3.0f*(u < 0 ? left : right);
                const float py = base - height*std::exp(-4.5f*u*u);
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            g.setColour(k == 0 ? white : blueLine.withMultipliedAlpha(juce::jlimit(0.25f, 1.0f, 1.0f - 0.07f*static_cast<float>(k))));
            g.strokePath(p, k == 0 ? thick : thin);
        }
        break;
    }
    case ModuleType::blur: // Blur: the input spectrum and the result smoothed by Width, scaled by Amount and Mix.
    {
        const float amount = values.raw("blurAmount"), width = values.normalised("blurWidth"), mix = values.raw("blurMix");
        constexpr int n = 140;
        std::array<float, n> sharp {}, soft {};
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>(i)/(n-1);
            const float env = 0.5f + 0.3f*std::sin(t*MathConstants<float>::pi*1.6f + 0.4f);
            sharp[static_cast<size_t>(i)] = env*0.72f + 0.22f*std::pow(std::abs(std::sin(t*41.0f)), 6.0f) + 0.08f*std::sin(t*97.0f);
        }
        const float s = 0.4f + width*12.0f;
        const int radius = static_cast<int>(std::ceil(3.0f*s));
        for (int i = 0; i < n; ++i)
        {
            float sum = 0, weight = 0;
            for (int j = -radius; j <= radius; ++j)
            {
                const float wgt = std::exp(-0.5f*static_cast<float>(j*j)/(s*s));
                sum += wgt*sharp[static_cast<size_t>(juce::jlimit(0, n-1, i+j))]; weight += wgt;
            }
            const float blurred = sum/weight, partial = sharp[static_cast<size_t>(i)] + amount*(blurred - sharp[static_cast<size_t>(i)]);
            soft[static_cast<size_t>(i)] = sharp[static_cast<size_t>(i)] + mix*(partial - sharp[static_cast<size_t>(i)]);
        }
        juce::Path input, output;
        for (int i = 0; i < n; ++i)
        {
            const float px = x0 + w*static_cast<float>(i)/(n-1);
            const float yi = y0 + h*(0.95f - sharp[static_cast<size_t>(i)]), yo = y0 + h*(0.95f - soft[static_cast<size_t>(i)]);
            if (i == 0) { input.startNewSubPath(px, yi); output.startNewSubPath(px, yo); } else { input.lineTo(px, yi); output.lineTo(px, yo); }
        }
        g.setColour(blueLine.withMultipliedAlpha(0.45f));
        g.strokePath(input, juce::PathStrokeType(1.0f));
        const float glow = amount*mix;
        for (int k = 3; k >= 1; --k)
        {
            g.setColour(blueLine.withMultipliedAlpha(0.22f*glow*static_cast<float>(4-k)));
            g.strokePath(output, juce::PathStrokeType(1.0f + (1.0f + 3.0f*width)*static_cast<float>(k), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(white);
        g.strokePath(output, thick);
        break;
    }
    case ModuleType::tremolo: // Tremolo: the engine's gain envelope (Rate, Depth, Shape), scrolling while enabled.
    {
        const float rate = values.raw("tremoloRate"), depth = values.raw("tremoloDepth"), shape = juce::jlimit(0.0f, 1.0f, values.raw("tremoloShape"));
        const float narrow = juce::jlimit(0.0f, 1.0f, 2*shape-1), duty = 0.5f - 0.375f*narrow*narrow*(3-2*narrow);
        const float edge = std::max(0.5f*std::pow(0.04f, std::min(2*shape, 1.0f)), rate*0.002f);
        // One second of signal, at most ten cycles; the scroll is slowed so fast rates stay readable.
        const float cycles = juce::jlimit(0.5f, 10.0f, rate);
        const double offset = enabled ? seconds*std::min(rate, 1.0f) : 0.0;
        const auto gain = [&](float t)
        {
            const double cycle = offset + static_cast<double>(t*cycles);
            const float distance = static_cast<float>(std::abs(cycle - std::floor(cycle + 0.5)));
            const float ramp = juce::jlimit(0.0f, 1.0f, (distance - 0.5f*(duty-edge))/edge);
            return 1.0f - depth*(1.0f - (0.5f + 0.5f*std::cos(MathConstants<float>::pi*ramp)));
        };
        const float cy = r.getCentreY(), amp = h*0.44f;
        for (int i = 0; i <= 56; ++i)
        {
            const float t = static_cast<float>(i)/56.0f, px = x0 + w*t, half = amp*gain(t);
            g.setColour(i % 2 ? blueLine.withMultipliedAlpha(0.6f) : blueLine);
            g.drawLine(px, cy-half, px, cy+half, 1.1f);
        }
        for (const float sign : {-1.0f, 1.0f})
        {
            juce::Path p;
            for (int i = 0; i <= 200; ++i)
            {
                const float t = static_cast<float>(i)/200.0f, px = x0 + w*t, py = cy + sign*amp*gain(t);
                if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
            }
            g.setColour(white);
            g.strokePath(p, thick);
        }
        break;
    }
    case ModuleType::shimmer: // Shimmer: a decaying tail (Decay, Size) with octave-up ramps (Shimmer, Tone), weighted by Mix.
    {
        const float decay = values.normalised("reverbDecay"), size = values.raw("reverbSize");
        const float shimmer = values.raw("reverbShimmer"), tone = values.raw("reverbTone"), mix = values.raw("reverbMix");
        const float base = y0 + h*0.92f, start = x0 + w*0.04f, end = x0 + w*(0.4f + 0.58f*decay);
        juce::Path tail;
        tail.startNewSubPath(start, base);
        for (int i = 0; i <= 80; ++i)
        {
            const float t = static_cast<float>(i)/80.0f, px = start + (end-start)*t;
            const float env = (1.0f - std::exp(-t*40.0f*(1.1f-size))) * std::exp(-t*4.0f);
            tail.lineTo(px, base - h*(0.25f + 0.35f*size)*env);
        }
        tail.lineTo(end, base); tail.closeSubPath();
        g.setGradientFill(juce::ColourGradient(blueLight.withAlpha((0.1f+0.3f*mix)*fade), start, base,
                                               blueLight.withAlpha(0.0f), end, base, false));
        g.fillPath(tail);
        const int ramps = 1 + juce::roundToInt(shimmer*6.0f);
        const float spacing = w*(0.03f + 0.07f*size);
        for (int k = ramps-1; k >= 0; --k)
        {
            const float sx = start + spacing*static_cast<float>(k);
            // Brighter Tone lets the octaves reach higher before they fade.
            const float reach = (0.35f + 0.6f*tone) * std::pow(0.9f, static_cast<float>(k));
            const float ex = std::min(x0+w, sx + (end-start)*(0.55f + 0.1f*static_cast<float>(k)));
            juce::Path p;
            p.startNewSubPath(sx, base);
            p.quadraticTo(sx + (ex-sx)*0.55f, base - h*0.02f, ex, base - h*0.85f*reach);
            g.setColour(k == 0 ? white.withMultipliedAlpha(0.6f + 0.4f*mix)
                               : blueLine.withMultipliedAlpha((1.0f - 0.1f*static_cast<float>(k))*(0.5f + 0.5f*mix)));
            g.strokePath(p, k == 0 ? thick : thin);
        }
        g.setColour(gridMajor.withMultipliedAlpha(fade));
        g.drawLine(x0, base, x0+w, base, 1.0f);
        break;
    }
    default: break;
    }
}
