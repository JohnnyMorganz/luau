// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Location.h"
#include "Luau/DenseHash.h"

#include <string>

namespace Luau {

extern int gCstRttiIndex;

template<typename T>
struct CstRtti
{
    static const int value;
};

template<typename T>
const int CstRtti<T>::value = ++gCstRttiIndex;

#define LUAU_CST_RTTI(Class) \
static int CstClassIndex() \
{ \
return CstRtti<Class>::value; \
}

class CstNode
{
public:
    explicit CstNode(int classIndex)
        : classIndex(classIndex)
    {
    }

//    virtual void visit(AstVisitor* visitor) = 0;
//
//    virtual AstExpr* asExpr()
//    {
//        return nullptr;
//    }
//    virtual AstStat* asStat()
//    {
//        return nullptr;
//    }
//    virtual AstType* asType()
//    {
//        return nullptr;
//    }
//    virtual AstAttr* asAttr()
//    {
//        return nullptr;
//    }

    template<typename T>
    bool is() const
    {
        return classIndex == T::CstClassIndex();
    }
    template<typename T>
    T* as()
    {
        return classIndex == T::CstClassIndex() ? static_cast<T*>(this) : nullptr;
    }
    template<typename T>
    const T* as() const
    {
        return classIndex == T::CstClassIndex() ? static_cast<const T*>(this) : nullptr;
    }

    const int classIndex;
};

class CstExprConstantNumber : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprConstantNumber)

    CstExprConstantNumber(const AstArray<char>& value);

    AstArray<char> value;
};

class CstExprConstantString : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprConstantNumber)

    enum QuoteStyle
    {
        QuotedSingle,
        QuotedDouble,
        QuotedRaw,
        QuotedInterp,
    };

    CstExprConstantString(AstArray<char> sourceString, QuoteStyle quoteStyle, unsigned int blockDepth);

    AstArray<char> sourceString;
    QuoteStyle quoteStyle;
    unsigned int blockDepth;
};

class CstExprCall : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprCall)

    CstExprCall(std::optional<Position> openParens, std::optional<Position> closeParens, AstArray<Position> commaPositions);

    std::optional<Position> openParens;
    std::optional<Position> closeParens;
    AstArray<Position> commaPositions;
};

class CstExprIndexExpr : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprIndexExpr)

    CstExprIndexExpr(Position openBracketPosition, Position closeBracketPosition);

    Position openBracketPosition;
    Position closeBracketPosition;
};

class CstExprFunction : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprFunction)

    CstExprFunction(AstArray<Position> argsCommaPositions);

    // TODO:
    //    Position openGenericsPosition;
    //    AstArray<Position> genericsCommaPositions;
    //    Position closeGenericsPosition;
    AstArray<Position> argsCommaPositions;
};

class CstExprTable : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprTable)

    enum Separator
    {
        Comma,
        Semicolon,
    };

    struct Item
    {
        // TODO: should we store positions rather than locations? we don't care about End, it can be inferred
        std::optional<Location> indexerOpenLocation;  // '[', only if Kind == General
        std::optional<Location> indexerCloseLocation; // ']', only if Kind == General
        std::optional<Location> equalsLocation;       // only if Kind != List
        std::optional<Separator> separator;           // may be missing for last Item
        std::optional<Location> separatorLocation;
    };

    CstExprTable(const AstArray<Item>& items);

    AstArray<Item> items;
};

// TODO: Shared between unary and binary, should we split?
class CstExprOp : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprOp)

    CstExprOp(Position opPosition);

    Position opPosition;
};

class CstStatDo : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatDo)

    CstStatDo(Position endPosition);

    Position endPosition;
};

class CstStatReturn : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatReturn)

    CstStatReturn(AstArray<Position> commaPositions);

    AstArray<Position> commaPositions;
};

class CstStatLocal : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatLocal)

    CstStatLocal(AstArray<Position> varsCommaPositions, AstArray<Position> valuesCommaPositions);

    AstArray<Position> varsCommaPositions;
    AstArray<Position> valuesCommaPositions;
};

class CstStatFor : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatFor)

    CstStatFor(Position equalsPosition, Position endCommaPosition, std::optional<Position> stepCommaPosition);

    Position equalsPosition;
    Position endCommaPosition;
    std::optional<Position> stepCommaPosition;
};

class CstStatForIn : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatForIn)

    CstStatForIn(AstArray<Position> varsCommaPositions, AstArray<Position> valuesCommaPositions);

    AstArray<Position> varsCommaPositions;
    AstArray<Position> valuesCommaPositions;
};

class CstStatAssign : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatAssign)

    CstStatAssign(AstArray<Position> varsCommaPositions, Position equalsPosition, AstArray<Position> valuesCommaPositions);

    AstArray<Position> varsCommaPositions;
    Position equalsPosition;
    AstArray<Position> valuesCommaPositions;
};

class CstStatCompoundAssign : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatCompoundAssign)

    CstStatCompoundAssign(Position opPosition);

    Position opPosition;
};

class CstStatLocalFunction : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatLocalFunction)

    CstStatLocalFunction(Position functionKeywordPosition);

    Position functionKeywordPosition;
};

class CstStatTypeAlias : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatTypeAlias)

    CstStatTypeAlias(Position typeKeywordPosition, AstArray<Position> genericsCommaPositions, Position equalsPosition);

    Position typeKeywordPosition;
    AstArray<Position> genericsCommaPositions;
    Position equalsPosition;
};

class CstStatTypeFunction : public CstNode
{
public:
    LUAU_CST_RTTI(CstStatTypeFunction)

    CstStatTypeFunction(Position typeKeywordPosition, Position functionKeywordPosition);

    Position typeKeywordPosition;
    Position functionKeywordPosition;
};

}