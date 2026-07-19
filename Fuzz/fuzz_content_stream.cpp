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

#include "pdfcms.h"
#include "pdfdocumentbuilder.h"
#include "pdffont.h"
#include "pdfoptionalcontent.h"
#include "pdfpagecontentprocessor.h"

#include <QByteArray>

namespace
{

struct FuzzPageContext
{
    pdf::PDFDocument document;
    const pdf::PDFPage* page = nullptr;
};

const FuzzPageContext& getFuzzContext()
{
    static const FuzzPageContext context = []
    {
        FuzzPageContext result;
        pdf::PDFDocumentBuilder builder;
        builder.createDocument();
        builder.appendPage(QRectF(0, 0, 400, 400));
        result.document = builder.build();
        result.page = result.document.getCatalog()->getPage(0);
        return result;
    }();

    return context;
}

}   // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    const FuzzPageContext& context = getFuzzContext();
    const pdf::PDFPage* page = context.page;
    if (!page)
    {
        return 0;
    }

    const pdf::PDFDocument* document = &context.document;
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFOptionalContentActivity optionalContentActivity(document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();
    pdf::PDFMeshQualitySettings meshQualitySettings;
    fontCache.setDocument(pdf::PDFModifiedDocument(document, &optionalContentActivity));
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFPageContentProcessor processor(page,
                                           document,
                                           &fontCache,
                                           cms.get(),
                                           &optionalContentActivity,
                                           QTransform(),
                                           meshQualitySettings);

    const QByteArray content(reinterpret_cast<const char*>(data), int(size));
    processor.processForm(QTransform(),
                          QRectF(0, 0, 400, 400),
                          pdf::PDFObject(),
                          pdf::PDFObject(),
                          content,
                          0);
    return 0;
}
