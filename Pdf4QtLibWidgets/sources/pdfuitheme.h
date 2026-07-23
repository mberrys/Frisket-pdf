// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PDFUITHEME_H
#define PDFUITHEME_H

#include "pdfwidgetsglobal.h"

#include <QColor>
#include <QPalette>
#include <QString>

namespace pdf
{

/// Shared semantic colors and palette helpers for Frisket PDF surfaces.
class PDF4QTLIBWIDGETSSHARED_EXPORT PDFUITheme
{
public:
    PDFUITheme() = delete;

    static constexpr int kDialogMarginPx = 12;
    static constexpr int kGroupBoxMarginPx = 20;
    static constexpr int kGroupBoxSpacingPx = 12;

    static QColor severityErrorColor();
    static QColor severityWarningColor();
    static QColor severityInfoColor();
    static QColor severityTextColor(const QString& severity);

    static QColor errorTextColor(const QPalette& palette);
    static QColor mutedTextColor(const QPalette& palette);
    static QColor canvasSurroundColor();
    static QColor findHighlightColor(const QPalette& palette, bool selected);
    static QColor bookmarkAutoColor(const QPalette& palette);
    static QColor bookmarkManualColor(const QPalette& palette);
    static QColor bookmarkSelectedColor(const QPalette& palette);

    static QString overlayLabelStyleSheet(const QPalette& palette);
    static QString errorLabelStyleSheet(const QPalette& palette);
};

} // namespace pdf

#endif // PDFUITHEME_H
