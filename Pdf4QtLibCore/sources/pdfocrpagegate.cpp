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

#include "pdfocrpagegate.h"

#include "pdfcatalog.h"
#include "pdfdocument.h"
#include "pdfdocumentsession.h"
#include "pdfdocumenttextflow.h"
#include "pdfpage.h"
#include "pdfpainter.h"

namespace pdf
{

namespace
{

int countNonWhitespaceCharacters(const QString& text)
{
    int count = 0;
    for (const QChar& character : text)
    {
        if (!character.isSpace())
        {
            ++count;
        }
    }
    return count;
}

bool pageHasImageContent(const PDFPrecompiledPage* compiledPage, const PDFPage* page)
{
    if (!compiledPage || !page)
    {
        return false;
    }

    const PDFPrecompiledPage::GraphicPieceInfos infos =
        compiledPage->calculateGraphicPieceInfos(page->getMediaBox(), 0.01);

    for (const PDFPrecompiledPage::GraphicPieceInfo& info : infos)
    {
        if (info.isImage())
        {
            return true;
        }
    }

    return false;
}

}   // namespace

PDFOcrPageGate::PageOcrNeed PDFOcrPageGate::classifyPage(PDFDocumentSession* session,
                                                         PDFInteger pageIndex,
                                                         const Settings& settings)
{
    if (!session || !session->isValid())
    {
        return PageOcrNeed::Failed;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return PageOcrNeed::Failed;
    }

    const PDFCatalog* catalog = document->getCatalog();
    if (!catalog || pageIndex < 0 || pageIndex >= PDFInteger(catalog->getPageCount()))
    {
        return PageOcrNeed::Failed;
    }

    PDFDocumentTextFlowFactory factory;
    const std::vector<PDFInteger> pageIndices = { pageIndex };
    const PDFDocumentTextFlow textFlow =
        factory.create(document, pageIndices, PDFDocumentTextFlowFactory::Algorithm::Layout);

    if (countNonWhitespaceCharacters(textFlow.getText()) >= settings.minTextCharacters)
    {
        return PageOcrNeed::SkipHasText;
    }

    const PDFPage* page = catalog->getPage(pageIndex);
    const PDFPrecompiledPage* compiledPage = session->compilePage(size_t(pageIndex));
    if (!page || !compiledPage)
    {
        return PageOcrNeed::Failed;
    }

    if (pageHasImageContent(compiledPage, page))
    {
        return PageOcrNeed::NeedsOcr;
    }

    return PageOcrNeed::SkipHasText;
}

}   // namespace pdf
