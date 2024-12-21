// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Ast.h"
#include "Luau/Cst.h"

namespace Luau
{

int gCstRttiIndex = 0;

CstExprConstantNumber::CstExprConstantNumber(const AstArray<char>& value)
    : CstNode(CstClassIndex())
    , value(value)
{
}

CstExprConstantString::CstExprConstantString(QuoteStyle quoteStyle, unsigned int blockDepth)
    : CstNode(CstClassIndex())
    , quoteStyle(quoteStyle)
    , blockDepth(blockDepth)
{
    LUAU_ASSERT(blockDepth == 0 || quoteStyle == QuoteStyle::QuotedRaw);
}

CstExprCall::CstExprCall(std::optional<Position> openParens, std::optional<Position> closeParens, AstArray<Position> commaPositions)
    : CstNode(CstClassIndex())
    , openParens(openParens)
    , closeParens(closeParens)
    , commaPositions(commaPositions)
{
}

CstExprTable::CstExprTable(const AstArray<Item>& items)
    : CstNode(CstClassIndex())
    , items(items)
{
}
}
