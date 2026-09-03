// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/Palettes.h"

#include <QColor>
#include <algorithm>
#include <cmath>
#include <vector>

namespace esdr3 {

namespace {

Palette makeClassic()
{
    Palette p;
    p.name = QStringLiteral("Classic");
    for (int i = 0; i < 256; ++i) {
        int r, g, b;
        if (i < 20) { r = 0; g = 0; b = 0; }
        else if (i < 70) { r = 0; g = 0; b = 140 * (i - 20) / 50; }
        else if (i < 100) { r = 60 * (i - 70) / 30; g = 125 * (i - 70) / 30; b = 115 * (i - 70) / 30 + 140; }
        else if (i < 150) { r = 195 * (i - 100) / 50 + 60; g = 130 * (i - 100) / 50 + 125; b = 255 - 255 * (i - 100) / 50; }
        else if (i < 250) { r = 255; g = 255 - 255 * (i - 150) / 100; b = 0; }
        else { r = 255; g = 255 * (i - 250) / 5; b = 255 * (i - 250) / 5; }
        p.table[size_t(i)] = qRgb(r, g, b);
    }
    return p;
}

Palette makeFromStops(const QString& name, const std::vector<std::pair<double, QColor>>& stops)
{
    Palette p;
    p.name = name;
    for (int i = 0; i < 256; ++i) {
        const double t = i / 255.0;
        size_t k = 0;
        while (k + 1 < stops.size() && stops[k + 1].first < t) ++k;
        const auto& a = stops[k];
        const auto& b = stops[std::min(k + 1, stops.size() - 1)];
        const double span = b.first - a.first;
        const double u = span > 0 ? std::clamp((t - a.first) / span, 0.0, 1.0) : 0.0;
        auto mix = [&](int ca, int cb) { return int(std::lround(ca + (cb - ca) * u)); };
        p.table[size_t(i)] = qRgb(mix(a.second.red(), b.second.red()),
                                  mix(a.second.green(), b.second.green()),
                                  mix(a.second.blue(), b.second.blue()));
    }
    return p;
}

const std::vector<Palette>& palettes()
{
    static const std::vector<Palette> list = {
        makeClassic(),
        makeFromStops(QStringLiteral("Winrad"), {
            {0.00, QColor(0, 0, 48)}, {0.25, QColor(0, 0, 160)}, {0.50, QColor(0, 160, 200)},
            {0.70, QColor(240, 240, 80)}, {0.88, QColor(255, 60, 0)}, {1.00, QColor(255, 255, 255)}}),
        makeFromStops(QStringLiteral("Amber"), {
            {0.00, QColor(0, 0, 0)}, {0.45, QColor(120, 50, 0)}, {0.75, QColor(255, 170, 0)},
            {1.00, QColor(255, 255, 220)}}),
        makeFromStops(QStringLiteral("Gray"), {
            {0.00, QColor(0, 0, 0)}, {1.00, QColor(255, 255, 255)}}),
    };
    return list;
}

}

QStringList paletteNames()
{
    QStringList names;
    for (const auto& p : palettes()) names << p.name;
    return names;
}

const Palette& paletteByName(const QString& name)
{
    for (const auto& p : palettes())
        if (p.name == name) return p;
    return palettes().front();
}

}
