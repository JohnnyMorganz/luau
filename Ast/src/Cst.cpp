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

CstExprConstantString::CstExprConstantString(AstArray<char> sourceString, QuoteStyle quoteStyle, unsigned int blockDepth)
    : CstNode(CstClassIndex())
    , sourceString(sourceString)
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

CstStatFor::CstStatFor(Position equalsPosition, Position endCommaPosition, std::optional<Position> stepCommaPosition)
    : CstNode(CstClassIndex())
    , equalsPosition(equalsPosition)
    , endCommaPosition(endCommaPosition)
    , stepCommaPosition(stepCommaPosition)
{
}
CstStatForIn::CstStatForIn(AstArray<Position> varsCommaPositions, AstArray<Position> valuesCommaPositions)
    : CstNode(CstClassIndex())
    , varsCommaPositions(varsCommaPositions)
    , valuesCommaPositions(valuesCommaPositions)
{
}
}
