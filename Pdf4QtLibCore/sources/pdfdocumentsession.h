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

#ifndef PDFDOCUMENTSESSION_H
#define PDFDOCUMENTSESSION_H

#include "pdfglobal.h"
#include "pdfdocument.h"
#include "pdfrenderer.h"
#include "pdfpainter.h"
#include "pdfcms.h"

#include <QByteArray>

#include <map>
#include <memory>

namespace pdf
{

class PDFFontCache;
class PDFCMSManager;
class PDFCMS;
class PDFOptionalContentActivity;

/// Shared session for a PDF document that caches expensive intermediate
/// artifacts used by preflight, rendering, and analysis tools. The session
/// borrows the document (it does not own it) and must be invalidated when the
/// document is mutated.
///
/// Current caches:
///   - Page compilation cache (`PDFPrecompiledPage` per page index)
///   - Decoded stream cache (`QByteArray` per stream object reference)
///
/// The session also owns the rendering helpers (font cache, CMS, optional
/// content activity, renderer) so that multiple checks can reuse them without
/// recreating them per page.
///
/// Thread-safety: the session is not thread-safe. Writes (compile, decode)
/// and invalidate() must not be called concurrently. Const reads from the
/// cache after writes complete are safe.
class PDF4QTLIBCORESHARED_EXPORT PDFDocumentSession
{
public:
    explicit PDFDocumentSession(PDFDocument* document);
    ~PDFDocumentSession();

    PDFDocumentSession(const PDFDocumentSession&) = delete;
    PDFDocumentSession& operator=(const PDFDocumentSession&) = delete;

    /// Returns the document associated with this session.
    PDFDocument* getDocument() const;

    /// Returns true if the session is associated with a non-null document.
    bool isValid() const;

    /// Sets the renderer features used when compiling pages. Changing features
    /// invalidates the compile cache because compiled pages depend on them.
    void setRendererFeatures(PDFRenderer::Features features);
    PDFRenderer::Features getRendererFeatures() const;

    /// Returns the precompiled page for the given page index, compiling it on
    /// first access and caching the result. Returns nullptr if the page does
    /// not exist or compilation fails.
    const PDFPrecompiledPage* compilePage(size_t pageIndex);

    /// Returns the decoded stream bytes for the given stream object reference,
    /// decoding it on first access and caching the result. Returns an empty
    /// byte array if the reference is invalid or the stream cannot be decoded.
    QByteArray getDecodedStream(PDFObjectReference reference);

    /// Clears all caches. Call this when the underlying document is mutated.
    void invalidate();

    /// Low-level access to the renderer and its dependencies. Prefer the
    /// compilePage() helper; these accessors are exposed for tools that need
    /// direct rasterization control.
    PDFRenderer* getRenderer() const;
    PDFFontCache* getFontCache() const;
    PDFCMS* getCMS() const;
    PDFOptionalContentActivity* getOptionalContentActivity() const;

private:
    void initializeRendering();

    PDFDocument* m_document;
    PDFRenderer::Features m_features;

    std::unique_ptr<PDFOptionalContentActivity> m_optionalContentActivity;
    std::unique_ptr<PDFCMSManager> m_cmsManager;
    PDFCMSPointer m_cms;
    std::unique_ptr<PDFFontCache> m_fontCache;
    std::unique_ptr<PDFRenderer> m_renderer;

    std::map<size_t, PDFPrecompiledPage> m_compileCache;
    std::map<PDFObjectReference, QByteArray> m_streamCache;
};

} // namespace pdf

#endif // PDFDOCUMENTSESSION_H
