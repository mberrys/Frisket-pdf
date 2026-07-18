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

#include "pdfdocumentsession.h"

#include "pdfconstants.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdfoptionalcontent.h"

namespace pdf
{

PDFDocumentSession::PDFDocumentSession(PDFDocument* document) :
    m_document(document),
    m_features(PDFRenderer::getDefaultFeatures())
{
    initializeRendering();
}

PDFDocumentSession::~PDFDocumentSession() = default;

PDFDocument* PDFDocumentSession::getDocument() const
{
    return m_document;
}

bool PDFDocumentSession::isValid() const
{
    return m_document != nullptr;
}

void PDFDocumentSession::setRendererFeatures(PDFRenderer::Features features)
{
    if (m_features == features)
    {
        return;
    }

    m_features = features;
    m_compileCache.clear();

    if (m_renderer)
    {
        PDFMeshQualitySettings meshQualitySettings;
        m_renderer = std::make_unique<PDFRenderer>(m_document,
                                                    m_fontCache.get(),
                                                    m_cms.get(),
                                                    m_optionalContentActivity.get(),
                                                    m_features,
                                                    meshQualitySettings);
    }
}

PDFRenderer::Features PDFDocumentSession::getRendererFeatures() const
{
    return m_features;
}

const PDFPrecompiledPage* PDFDocumentSession::compilePage(size_t pageIndex)
{
    if (!isValid())
    {
        return nullptr;
    }

    auto it = m_compileCache.find(pageIndex);
    if (it != m_compileCache.cend())
    {
        return &it->second;
    }

    const PDFCatalog* catalog = m_document->getCatalog();
    if (!catalog || pageIndex >= static_cast<size_t>(catalog->getPageCount()))
    {
        return nullptr;
    }

    PDFPrecompiledPage compiledPage;
    m_renderer->compile(&compiledPage, pageIndex);

    auto result = m_compileCache.emplace(pageIndex, std::move(compiledPage));
    return &result.first->second;
}

QByteArray PDFDocumentSession::getDecodedStream(PDFObjectReference reference)
{
    if (!isValid())
    {
        return QByteArray();
    }

    auto it = m_streamCache.find(reference);
    if (it != m_streamCache.cend())
    {
        return it->second;
    }

    const PDFObject& object = m_document->getObjectByReference(reference);
    if (!object.isStream())
    {
        return QByteArray();
    }

    QByteArray decoded = m_document->getStorage().getDecodedStream(object.getStream());
    auto result = m_streamCache.emplace(reference, decoded);
    return result.first->second;
}

void PDFDocumentSession::invalidate()
{
    m_compileCache.clear();
    m_streamCache.clear();
}

PDFRenderer* PDFDocumentSession::getRenderer() const
{
    return m_renderer.get();
}

PDFFontCache* PDFDocumentSession::getFontCache() const
{
    return m_fontCache.get();
}

PDFCMS* PDFDocumentSession::getCMS() const
{
    return m_cms.get();
}

PDFOptionalContentActivity* PDFDocumentSession::getOptionalContentActivity() const
{
    return m_optionalContentActivity.get();
}

void PDFDocumentSession::initializeRendering()
{
    if (!isValid())
    {
        return;
    }

    m_optionalContentActivity = std::make_unique<PDFOptionalContentActivity>(m_document, OCUsage::Export, nullptr);

    m_cmsManager = std::make_unique<PDFCMSManager>(nullptr);
    m_cmsManager->setDocument(m_document);
    m_cms = m_cmsManager->getCurrentCMS();

    m_fontCache = std::make_unique<PDFFontCache>(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument modifiedDocument(m_document, m_optionalContentActivity.get());
    m_fontCache->setDocument(modifiedDocument);
    m_fontCache->setCacheShrinkEnabled(nullptr, false);

    PDFMeshQualitySettings meshQualitySettings;
    m_renderer = std::make_unique<PDFRenderer>(m_document,
                                                m_fontCache.get(),
                                                m_cms.get(),
                                                m_optionalContentActivity.get(),
                                                m_features,
                                                meshQualitySettings);
}

} // namespace pdf
