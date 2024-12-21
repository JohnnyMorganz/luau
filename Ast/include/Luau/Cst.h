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

    CstExprConstantString(QuoteStyle quoteStyle, unsigned int blockDepth);

    QuoteStyle quoteStyle;
    unsigned int blockDepth;
};

class CstExprCall : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprCall)

    CstExprCall(bool hasLeadingParens, bool hasTrailingParens);

    bool hasLeadingParens;
    bool hasTrailingParens;
};

}