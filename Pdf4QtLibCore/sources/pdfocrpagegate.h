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

#ifndef PDFOCRPAGEGATE_H
#define PDFOCRPAGEGATE_H

#include "pdfglobal.h"

namespace pdf
{

class PDFDocument;
class PDFDocumentSession;

class PDF4QTLIBCORESHARED_EXPORT PDFOcrPageGate
{
public:
    enum class PageOcrNeed
    {
        SkipHasText,
        SkipEmpty,
        NeedsOcr,
        Failed
    };

    struct Settings
    {
        int minTextCharacters = 20;
    };

    static PageOcrNeed classifyPage(PDFDocumentSession* session,
                                    PDFInteger pageIndex,
                                    const Settings& settings);
};

}   // namespace pdf

#endif // PDFOCRPAGEGATE_H
