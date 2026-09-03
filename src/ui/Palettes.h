// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QRgb>
#include <QString>
#include <QStringList>
#include <array>

namespace esdr3 {

struct Palette {
    QString name;
    std::array<QRgb, 256> table;
};

QStringList paletteNames();
const Palette& paletteByName(const QString& name);

}
