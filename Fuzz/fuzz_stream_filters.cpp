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

#include "pdfstreamfilters.h"

#include <QByteArray>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
    {
        return 0;
    }

    static const char* filters[] = {
        "FlateDecode",
        "LZWDecode",
        "ASCII85Decode",
        "ASCIIHexDecode",
        "RunLengthDecode",
    };

    const size_t filterIndex = data[0] % (sizeof(filters) / sizeof(filters[0]));
    const pdf::PDFStreamFilter* filter = pdf::PDFStreamFilterStorage::getFilter(filters[filterIndex]);
    if (!filter)
    {
        return 0;
    }

    const QByteArray payload(reinterpret_cast<const char*>(data + 1), int(size - 1));
    (void)filter->apply(payload, pdf::PDFObject(), nullptr);
    return 0;
}
