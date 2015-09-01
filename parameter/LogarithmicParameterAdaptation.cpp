/*
 * Copyright (c) 2011-2015, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LogarithmicParameterAdaptation.h"
#include "Utility.h"
#include <cmath>

#define base CLinearParameterAdaptation

CLogarithmicParameterAdaptation::CLogarithmicParameterAdaptation() : base("Logarithmic"),
    // std::exp(1) == e^1 == e == natural logarithm base
    // Make sure there is no precision lose by using std::exp overload that
    // return the same type as _dLogarithmBase
    _dLogarithmBase{std::exp(decltype(_dLogarithmBase){1})},
    _dFloorValue(-INFINITY)
{
}

// Element properties
void CLogarithmicParameterAdaptation::showProperties(std::string& strResult) const
{
    base::showProperties(strResult);

    strResult += " - LogarithmBase: ";
    strResult += CUtility::toString(_dLogarithmBase);
    strResult += "\n";
    strResult += " - FloorValue: ";
    strResult += CUtility::toString(_dFloorValue);
    strResult += "\n";
}

bool CLogarithmicParameterAdaptation::fromXml(const CXmlElement& xmlElement,
                                            CXmlSerializingContext& serializingContext)
{
    if (xmlElement.getAttribute("LogarithmBase", _dLogarithmBase)
        && (_dLogarithmBase <= 0 || _dLogarithmBase == 1)) {
        // Avoid negative and 1 values
        serializingContext.setError("LogarithmBase attribute cannot be negative or 1 on element"
                                    + xmlElement.getPath());

        return false;
    }

    xmlElement.getAttribute("FloorValue", _dFloorValue);

    // Base
    return base::fromXml(xmlElement, serializingContext);
}


int64_t CLogarithmicParameterAdaptation::fromUserValue(double dValue) const
{
    return fmax(round(base::fromUserValue(log(dValue) / log(_dLogarithmBase))),
                        _dFloorValue);
}

double CLogarithmicParameterAdaptation::toUserValue(int64_t iValue) const
{
    return exp(base::toUserValue(iValue) * log(_dLogarithmBase));
}
