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

#include "pdfccittfaxdecoder.h"
#include "pdfexception.h"
#include "pdfjbig2decoder.h"

#include <QByteArray>

#include <algorithm>
#include <cstdint>
#include <exception>

namespace
{

constexpr int kMaxColumns = 512;
constexpr int kMaxRows = 512;

int clampDimension(int value, int fallback, int maxValue)
{
    if (value <= 0)
    {
        return fallback;
    }

    return std::min(value, maxValue);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 8)
    {
        return 0;
    }

    const uint8_t selector = data[0];
    const QByteArray payload(reinterpret_cast<const char*>(data + 1), int(size - 1));

    try
    {
        if ((selector & 0x1u) == 0u)
        {
            pdf::PDFCCITTFaxDecoderParameters parameters;
            parameters.K = static_cast<pdf::PDFInteger>(static_cast<int8_t>(data[1]));
            parameters.columns = clampDimension(data[2] | (data[3] << 8), 64, kMaxColumns);
            parameters.rows = clampDimension(data[4], 64, kMaxRows);
            parameters.hasEndOfLine = (data[5] & 0x01u) != 0u;
            parameters.hasEncodedByteAlign = (data[5] & 0x02u) != 0u;
            parameters.hasEndOfBlock = (data[5] & 0x04u) != 0u;
            parameters.hasBlackIsOne = (data[5] & 0x08u) != 0u;

            const QByteArray stream = payload.mid(5);
            if (stream.isEmpty())
            {
                return 0;
            }

            pdf::PDFCCITTFaxDecoder decoder(&stream, parameters);
            (void)decoder.decode();
        }
        else
        {
            pdf::PDFRenderErrorReporterDummy errorReporter;
            const int split = data[1] % static_cast<int>(std::max<size_t>(1, payload.size()));
            const QByteArray pageData = payload.mid(1, split);
            const QByteArray globalData = payload.mid(1 + split);

            pdf::PDFJBIG2Decoder decoder(pageData, globalData, &errorReporter);
            if ((selector & 0x2u) == 0u)
            {
                (void)decoder.decode(pdf::PDFImageData::MaskingType::None);
            }
            else
            {
                (void)decoder.decodeFileStream();
            }
        }
    }
    catch (const pdf::PDFException&)
    {
    }
    catch (const std::exception&)
    {
    }
    catch (...)
    {
    }

    return 0;
}
