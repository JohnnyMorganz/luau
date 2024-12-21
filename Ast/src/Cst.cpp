// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Ast.h"
#include "Luau/Cst.h"

namespace Luau
{

int gCstRttiIndex = 0;

CstExprConstantNumber::CstExprConstantNumber(const Location& location, const AstArray<char>& value) : CstNode(CstClassIndex(), location), value(value)
{
}

CstExprConstantString::CstExprConstantString(const Location& location, QuoteStyle quoteStyle, unsigned int blockDepth) : CstNode(CstClassIndex(), location), quoteStyle(quoteStyle), blockDepth(blockDepth)
{
    LUAU_ASSERT(blockDepth == 0 || quoteStyle == QuoteStyle::QuotedRaw);
}

}
