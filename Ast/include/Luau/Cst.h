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
    explicit CstNode(int classIndex, const Location& location)
        : classIndex(classIndex)
        , location(location)
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
    Location location;
};

class CstExprConstantNumber : public CstNode
{
public:
    LUAU_CST_RTTI(CstExprConstantNumber)

    CstExprConstantNumber(const Location& location, const AstArray<char>& value);

    AstArray<char> value;
};

}