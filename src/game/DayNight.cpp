#include "DayNight.h"
#include <cmath>
#include <cstdio>

namespace {
// direction TO the sun for a given hour (sun rises in +X, sets in -X)
Vec3 sunDirection(float hour) {
    float a = (hour - 6.0f) / 12.0f * PI;
    return normalize(Vec3(std::cos(a) * 0.95f, std::sin(a), 0.42f));
}
Vec3 lerp3(const Vec3& a, const Vec3& b, float t) { return mix(a, b, clampf(t, 0.0f, 1.0f)); }
}  // namespace

void DayNight::update(float dt) {
    if (paused) return;
    hour += dt * hoursPerSecond;
    if (hour >= 24.0f) hour -= 24.0f;
}

float DayNight::sunHeight() const { return sunDirection(hour).y; }

float DayNight::night() const { return 1.0f - smoothstepf(-0.14f, 0.10f, sunHeight()); }

void DayNight::toggleDayNight() {
    if (night() > 0.5f) hour = 9.0f;
    else hour = 20.5f;
}

std::string DayNight::clockText() const {
    int h = (int)hour, m = (int)((hour - h) * 60.0f);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d %s", (h % 12 == 0) ? 12 : h % 12, m, h < 12 ? "AM" : "PM");
    return buf;
}

const char* DayNight::phaseName() const {
    float s = sunHeight();
    if (s > 0.25f) return "Day";
    if (s > 0.0f) return hour < 12 ? "Sunrise" : "Sunset";
    if (s > -0.14f) return "Twilight";
    return "Night";
}

void DayNight::fillEnvironment(Environment& e) const {
    Vec3 sun = sunDirection(hour);
    float s = sun.y;
    float day = smoothstepf(-0.10f, 0.25f, s);          // 0 night .. 1 bright day
    float sunset = std::exp(-std::pow((s - 0.04f) / 0.13f, 2.0f));  // peaks near horizon
    float nightAmt = night();

    e.sunDir = sun;
    e.moonDir = normalize(Vec3(-sun.x * 0.8f, -sun.y, 0.30f));
    e.night = nightAmt;

    // ---- directional light: sun by day, moon by night ----
    if (s > -0.02f) {
        Vec3 warm(1.0f, 0.62f, 0.38f), white(1.0f, 0.96f, 0.88f);
        Vec3 col = lerp3(warm, white, smoothstepf(0.02f, 0.45f, s));
        e.lightDir = sun;
        e.lightColor = col * (1.15f * smoothstepf(-0.02f, 0.18f, s));
        e.castShadows = s > 0.03f;
    } else {
        e.lightDir = e.moonDir;
        e.lightColor = Vec3(0.30f, 0.38f, 0.60f) * (0.48f * smoothstepf(-0.02f, -0.20f, s));
        e.castShadows = false;   // moon shadows are very faint: skipped to save GPU time
    }

    // ---- sky colours ----
    Vec3 zenNight(0.008f, 0.012f, 0.035f), zenTwi(0.10f, 0.13f, 0.32f), zenDay(0.20f, 0.42f, 0.82f);
    Vec3 horNight(0.025f, 0.035f, 0.07f), horSet(0.98f, 0.52f, 0.28f), horDay(0.66f, 0.79f, 0.92f);
    Vec3 zen = lerp3(lerp3(zenNight, zenTwi, smoothstepf(-0.20f, -0.02f, s)), zenDay, smoothstepf(0.0f, 0.35f, s));
    Vec3 hor = lerp3(horNight, horDay, day);
    hor = lerp3(hor, horSet, sunset * 0.85f);
    e.zenith = zen;
    e.horizon = hor;
    e.groundSky = lerp3(Vec3(0.02f, 0.025f, 0.03f), Vec3(0.28f, 0.33f, 0.30f), day);
    e.sunGlow = lerp3(Vec3(1.0f, 0.75f, 0.45f), Vec3(1.0f, 0.95f, 0.85f), smoothstepf(0.05f, 0.4f, s)) * smoothstepf(-0.1f, 0.05f, s);
    e.cloudColor = lerp3(Vec3(0.05f, 0.06f, 0.09f), Vec3(0.97f, 0.97f, 1.0f), day);
    e.cloudColor = lerp3(e.cloudColor, Vec3(1.0f, 0.62f, 0.48f), sunset * 0.7f);

    // ---- ambient light (hemisphere) ----
    e.skyAmbient = lerp3(Vec3(0.065f, 0.078f, 0.12f), Vec3(0.40f, 0.45f, 0.54f), day);
    e.skyAmbient = lerp3(e.skyAmbient, Vec3(0.42f, 0.32f, 0.30f), sunset * 0.4f);
    e.groundAmbient = lerp3(Vec3(0.035f, 0.038f, 0.045f), Vec3(0.20f, 0.22f, 0.16f), day);

    // ---- fog (jungle haze) ----
    e.fogColor = lerp3(Vec3(0.02f, 0.03f, 0.05f), lerp3(hor, Vec3(0.62f, 0.70f, 0.70f), 0.35f), day);
    e.fogColor = lerp3(e.fogColor, hor * 0.85f, sunset * 0.5f);
    e.fogDensity = lerpf(0.0125f, 0.0072f, day);
    e.exposure = lerpf(1.75f, 1.25f, day);
}
