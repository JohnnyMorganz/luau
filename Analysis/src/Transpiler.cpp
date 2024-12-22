// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/Transpiler.h"

#include "Luau/Parser.h"
#include "Luau/StringUtils.h"
#include "Luau/Common.h"

#include <algorithm>
#include <memory>
#include <limits>
#include <math.h>


namespace
{
bool isIdentifierStartChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool isIdentifierChar(char c)
{
    return isIdentifierStartChar(c) || isDigit(c);
}

const std::vector<std::string> keywords = {"and",   "break", "do",  "else", "elseif", "end",    "false", "for",  "function", "if",   "in",
                                           "local", "nil",   "not", "or",   "repeat", "return", "then",  "true", "until",    "while"};

} // namespace

namespace Luau
{

struct Writer
{
    virtual ~Writer() {}

    virtual void advance(const Position&) = 0;
    virtual void newline() = 0;
    virtual void space() = 0;
    virtual void maybeSpace(const Position& newPos, int reserve) = 0;
    virtual void write(std::string_view) = 0;
    virtual void identifier(std::string_view name) = 0;
    virtual void keyword(std::string_view) = 0;
    virtual void symbol(std::string_view) = 0;
    virtual void literal(std::string_view) = 0;
    virtual void string(std::string_view) = 0;
    virtual void sourceString(std::string_view, CstExprConstantString::QuoteStyle quoteStyle, unsigned int blockDepth) = 0;
};

struct StringWriter : Writer
{
    std::string ss;
    Position pos{0, 0};
    char lastChar = '\0'; // used to determine whether we need to inject an extra space to preserve grammatical correctness.

    const std::string& str() const
    {
        return ss;
    }

    void advance(const Position& newPos) override
    {
        while (pos.line < newPos.line)
            newline();

        if (pos.column < newPos.column)
            write(std::string(newPos.column - pos.column, ' '));
    }

    void maybeSpace(const Position& newPos, int reserve) override
    {
        if (pos.column + reserve < newPos.column)
            space();
    }

    void newline() override
    {
        ss += '\n';
        pos.column = 0;
        ++pos.line;
        lastChar = '\n';
    }

    void space() override
    {
        ss += ' ';
        ++pos.column;
        lastChar = ' ';
    }

    void write(std::string_view s) override
    {
        if (s.empty())
            return;

        ss.append(s.data(), s.size());
        pos.column += unsigned(s.size());
        lastChar = s[s.size() - 1];
    }

    void write(char c)
    {
        ss += c;
        pos.column += 1;
        lastChar = c;
    }

    void identifier(std::string_view s) override
    {
        if (s.empty())
            return;

        if (isIdentifierChar(lastChar))
            space();

        write(s);
    }

    void keyword(std::string_view s) override
    {
        if (s.empty())
            return;

        if (isIdentifierChar(lastChar))
            space();

        write(s);
    }

    void symbol(std::string_view s) override
    {
        if (isDigit(lastChar) && s[0] == '.')
            space();

        write(s);
    }

    void literal(std::string_view s) override
    {
        if (s.empty())
            return;

        else if (isIdentifierChar(lastChar) && isDigit(s[0]))
            space();

        write(s);
    }

    void string(std::string_view s) override
    {
        char quote = '\'';
        if (std::string::npos != s.find(quote))
            quote = '\"';

        write(quote);
        write(escape(s));
        write(quote);
    }

    void sourceString(std::string_view s, CstExprConstantString::QuoteStyle quoteStyle, unsigned int blockDepth) override
    {
        if (quoteStyle == CstExprConstantString::QuotedRaw)
        {
            auto blocks = std::string(blockDepth, '=');
            write('[');
            write(blocks);
            write('[');
            write(s);
            write(']');
            write(blocks);
            write(']');
        }
        else
        {
            LUAU_ASSERT(blockDepth == 0);

            char quote = '"';
            switch (quoteStyle)
            {
            case CstExprConstantString::QuotedDouble:
                quote = '"';
                break;
            case CstExprConstantString::QuotedSingle:
                quote = '\'';
                break;
            case CstExprConstantString::QuotedInterp:
                quote = '`';
                break;
            default:
                LUAU_ASSERT(!"Unhandled quote type");
            }

            write(quote);
            write(s);
            write(quote);
        }
    }
};

class CommaSeparatorInserter
{
public:
    CommaSeparatorInserter(Writer& w, const Position* commaPosition = nullptr)
        : first(true)
        , writer(w)
        , commaPosition(commaPosition)
    {
    }
    void operator()()
    {
        if (first)
            first = !first;
        else
        {
            if (commaPosition)
            {
                writer.advance(*commaPosition);
                commaPosition++;
            }
            writer.symbol(",");
        }
    }

private:
    bool first;
    Writer& writer;
    const Position* commaPosition;
};

struct Printer
{
    explicit Printer(Writer& writer, const CstNodeMap& cstNodeMap)
        : writer(writer),
        cstNodeMap(cstNodeMap)
    {
    }

    bool writeTypes = false;
    Writer& writer;
    CstNodeMap cstNodeMap;

    void visualize(const AstLocal& local)
    {
        advance(local.location.begin);

        writer.identifier(local.name.value);
        if (writeTypes && local.annotation)
        {
            writer.symbol(":");
            visualizeTypeAnnotation(*local.annotation);
        }
    }

    void visualizeTypePackAnnotation(const AstTypePack& annotation, bool forVarArg)
    {
        advance(annotation.location.begin);
        if (const AstTypePackVariadic* variadicTp = annotation.as<AstTypePackVariadic>())
        {
            if (!forVarArg)
                writer.symbol("...");

            visualizeTypeAnnotation(*variadicTp->variadicType);
        }
        else if (const AstTypePackGeneric* genericTp = annotation.as<AstTypePackGeneric>())
        {
            writer.symbol(genericTp->genericName.value);
            writer.symbol("...");
        }
        else if (const AstTypePackExplicit* explicitTp = annotation.as<AstTypePackExplicit>())
        {
            LUAU_ASSERT(!forVarArg);
            visualizeTypeList(explicitTp->typeList, true);
        }
        else
        {
            LUAU_ASSERT(!"Unknown TypePackAnnotation kind");
        }
    }

    void visualizeTypeList(const AstTypeList& list, bool unconditionallyParenthesize)
    {
        size_t typeCount = list.types.size + (list.tailType != nullptr ? 1 : 0);
        if (typeCount == 0)
        {
            writer.symbol("(");
            writer.symbol(")");
        }
        else if (typeCount == 1)
        {
            if (unconditionallyParenthesize)
                writer.symbol("(");

            // Only variadic tail
            if (list.types.size == 0)
            {
                visualizeTypePackAnnotation(*list.tailType, false);
            }
            else
            {
                visualizeTypeAnnotation(*list.types.data[0]);
            }

            if (unconditionallyParenthesize)
                writer.symbol(")");
        }
        else
        {
            writer.symbol("(");

            bool first = true;
            for (const auto& el : list.types)
            {
                if (first)
                    first = false;
                else
                    writer.symbol(",");

                visualizeTypeAnnotation(*el);
            }

            if (list.tailType)
            {
                writer.symbol(",");
                visualizeTypePackAnnotation(*list.tailType, false);
            }

            writer.symbol(")");
        }
    }

    bool isIntegerish(double d)
    {
        if (d <= std::numeric_limits<int>::max() && d >= std::numeric_limits<int>::min())
            return double(int(d)) == d && !(d == 0.0 && signbit(d));
        else
            return false;
    }

    void visualize(AstExpr& expr)
    {
        advance(expr.location.begin);

        if (const auto& a = expr.as<AstExprGroup>())
        {
            writer.symbol("(");
            visualize(*a->expr);
            writer.symbol(")");
        }
        else if (expr.is<AstExprConstantNil>())
        {
            writer.keyword("nil");
        }
        else if (const auto& a = expr.as<AstExprConstantBool>())
        {
            if (a->value)
                writer.keyword("true");
            else
                writer.keyword("false");
        }
        else if (const auto& a = expr.as<AstExprConstantNumber>())
        {
            if (const auto c = cstNodeMap[a]){
                auto x = c->as<CstExprConstantNumber>();
                writer.literal(std::string_view(x->value.data, x->value.size));
            }
            else
            {
                if (isinf(a->value))
                {
                    if (a->value > 0)
                        writer.literal("1e500");
                    else
                        writer.literal("-1e500");
                }
                else if (isnan(a->value))
                    writer.literal("0/0");
                else
                {
                    if (isIntegerish(a->value))
                        writer.literal(std::to_string(int(a->value)));
                    else
                    {
                        char buffer[100];
                        size_t len = snprintf(buffer, sizeof(buffer), "%.17g", a->value);
                        writer.literal(std::string_view{buffer, len});
                    }
                }
            }
        }
        else if (const auto& a = expr.as<AstExprConstantString>())
        {
            if (const auto c = cstNodeMap[a])
            {
                auto cstNode = c->as<CstExprConstantString>();
                writer.sourceString(
                    std::string_view(cstNode->sourceString.data, cstNode->sourceString.size), cstNode->quoteStyle, cstNode->blockDepth
                );
            }
            else
                writer.string(std::string_view(a->value.data, a->value.size));
        }
        else if (const auto& a = expr.as<AstExprLocal>())
        {
            writer.identifier(a->local->name.value);
        }
        else if (const auto& a = expr.as<AstExprGlobal>())
        {
            writer.identifier(a->name.value);
        }
        else if (expr.is<AstExprVarargs>())
        {
            writer.symbol("...");
        }
        else if (const auto& a = expr.as<AstExprCall>())
        {
            visualize(*a->func);

            const CstExprCall* cstNode = nullptr;
            if (auto c = cstNodeMap[a])
                cstNode = c->as<CstExprCall>();

            if (cstNode)
            {
                if (cstNode->openParens)
                {
                    advance(*cstNode->openParens);
                    writer.symbol("(");
                }
            }
            else
            {
                writer.symbol("(");
            }

            CommaSeparatorInserter comma(writer, cstNode ? cstNode->commaPositions.begin() : nullptr);
            for (const auto& arg : a->args)
            {
                comma();
                visualize(*arg);
            }

            if (cstNode)
            {
                if (cstNode->closeParens)
                {
                    advance(*cstNode->closeParens);
                    writer.symbol(")");
                }
            }
            else
            {
                writer.symbol(")");
            }
        }
        else if (const auto& a = expr.as<AstExprIndexName>())
        {
            visualize(*a->expr);
            writer.symbol(std::string(1, a->op));
            writer.write(a->index.value);
        }
        else if (const auto& a = expr.as<AstExprIndexExpr>())
        {
            visualize(*a->expr);
            writer.symbol("[");
            visualize(*a->index);
            writer.symbol("]");
        }
        else if (const auto& a = expr.as<AstExprFunction>())
        {
            writer.keyword("function");
            visualizeFunctionBody(*a);
        }
        else if (const auto& a = expr.as<AstExprTable>())
        {
            writer.symbol("{");

            const CstExprTable::Item* cstItem = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstItem = c->as<CstExprTable>()->items.begin();

            bool first = true;

            for (const auto& item : a->items)
            {
                if (!cstItem)
                {
                    if (first)
                        first = false;
                    else
                        writer.symbol(",");
                }

                switch (item.kind)
                {
                case AstExprTable::Item::List:
                    break;

                case AstExprTable::Item::Record:
                {
                    const auto& value = item.key->as<AstExprConstantString>()->value;
                    advance(item.key->location.begin);
                    writer.identifier(std::string_view(value.data, value.size));
                    if (cstItem)
                        advance(cstItem->equalsLocation->begin);
                    else
                        writer.maybeSpace(item.value->location.begin, 1);
                    writer.symbol("=");
                }
                break;

                case AstExprTable::Item::General:
                {
                    if (cstItem)
                        advance(cstItem->indexerOpenLocation->begin);
                    writer.symbol("[");
                    visualize(*item.key);
                    if (cstItem)
                        advance(cstItem->indexerCloseLocation->begin);
                    writer.symbol("]");
                    if (cstItem)
                        advance(cstItem->equalsLocation->begin);
                    else
                        writer.maybeSpace(item.value->location.begin, 1);
                    writer.symbol("=");
                }
                break;

                default:
                    LUAU_ASSERT(!"Unknown table item kind");
                }

                advance(item.value->location.begin);
                visualize(*item.value);

                if (cstItem)
                {
                    advance(cstItem->separatorLocation->begin);
                    if (cstItem->separator == CstExprTable::Comma)
                        writer.symbol(",");
                    else if (cstItem->separator == CstExprTable::Semicolon)
                        writer.symbol(";");
                    cstItem++;
                }
            }

            Position endPos = expr.location.end;
            if (endPos.column > 0)
                --endPos.column;

            advance(endPos);

            writer.symbol("}");
            advance(expr.location.end);
        }
        else if (const auto& a = expr.as<AstExprUnary>())
        {
            switch (a->op)
            {
            case AstExprUnary::Not:
                writer.keyword("not");
                break;
            case AstExprUnary::Minus:
                writer.symbol("-");
                break;
            case AstExprUnary::Len:
                writer.symbol("#");
                break;
            }
            visualize(*a->expr);
        }
        else if (const auto& a = expr.as<AstExprBinary>())
        {
            visualize(*a->left);

            switch (a->op)
            {
            case AstExprBinary::Add:
            case AstExprBinary::Sub:
            case AstExprBinary::Mul:
            case AstExprBinary::Div:
            case AstExprBinary::FloorDiv:
            case AstExprBinary::Mod:
            case AstExprBinary::Pow:
            case AstExprBinary::CompareLt:
            case AstExprBinary::CompareGt:
                writer.maybeSpace(a->right->location.begin, 2);
                writer.symbol(toString(a->op));
                break;
            case AstExprBinary::Concat:
            case AstExprBinary::CompareNe:
            case AstExprBinary::CompareEq:
            case AstExprBinary::CompareLe:
            case AstExprBinary::CompareGe:
            case AstExprBinary::Or:
                writer.maybeSpace(a->right->location.begin, 3);
                writer.keyword(toString(a->op));
                break;
            case AstExprBinary::And:
                writer.maybeSpace(a->right->location.begin, 4);
                writer.keyword(toString(a->op));
                break;
            default:
                LUAU_ASSERT(!"Unknown Op");
            }

            visualize(*a->right);
        }
        else if (const auto& a = expr.as<AstExprTypeAssertion>())
        {
            visualize(*a->expr);

            if (writeTypes)
            {
                writer.maybeSpace(a->annotation->location.begin, 2);
                writer.symbol("::");
                visualizeTypeAnnotation(*a->annotation);
            }
        }
        else if (const auto& a = expr.as<AstExprIfElse>())
        {
            writer.keyword("if");
            visualize(*a->condition);
            writer.keyword("then");
            visualize(*a->trueExpr);
            writer.keyword("else");
            visualize(*a->falseExpr);
        }
        else if (const auto& a = expr.as<AstExprInterpString>())
        {
            writer.symbol("`");

            size_t index = 0;

            for (const auto& string : a->strings)
            {
                writer.write(escape(std::string_view(string.data, string.size), /* escapeForInterpString = */ true));

                if (index < a->expressions.size)
                {
                    writer.symbol("{");
                    visualize(*a->expressions.data[index]);
                    writer.symbol("}");
                }

                index++;
            }

            writer.symbol("`");
        }
        else if (const auto& a = expr.as<AstExprError>())
        {
            writer.symbol("(error-expr");

            for (size_t i = 0; i < a->expressions.size; i++)
            {
                writer.symbol(i == 0 ? ": " : ", ");
                visualize(*a->expressions.data[i]);
            }

            writer.symbol(")");
        }
        else
        {
            LUAU_ASSERT(!"Unknown AstExpr");
        }
    }

    void writeEnd(const Location& loc)
    {
        Position endPos = loc.end;
        if (endPos.column >= 3)
            endPos.column -= 3;
        advance(endPos);
        writer.keyword("end");
    }

    void advance(const Position& newPos)
    {
        writer.advance(newPos);
    }

    void visualize(AstStat& program)
    {
        advance(program.location.begin);

        if (const auto& block = program.as<AstStatBlock>())
        {
            writer.keyword("do");
            for (const auto& s : block->body)
                visualize(*s);
            writer.advance(block->location.end);
            writeEnd(program.location);
        }
        else if (const auto& a = program.as<AstStatIf>())
        {
            writer.keyword("if");
            visualizeElseIf(*a);
        }
        else if (const auto& a = program.as<AstStatWhile>())
        {
            writer.keyword("while");
            visualize(*a->condition);
            // TODO: what if 'hasDo = false'?
            advance(a->doLocation.begin);
            writer.keyword("do");
            visualizeBlock(*a->body);
            writeEnd(program.location);
        }
        else if (const auto& a = program.as<AstStatRepeat>())
        {
            writer.keyword("repeat");
            visualizeBlock(*a->body);
            if (a->condition->location.begin.column > 5)
                writer.advance(Position{a->condition->location.begin.line, a->condition->location.begin.column - 6});
            writer.keyword("until");
            visualize(*a->condition);
        }
        else if (program.is<AstStatBreak>())
            writer.keyword("break");
        else if (program.is<AstStatContinue>())
            writer.keyword("continue");
        else if (const auto& a = program.as<AstStatReturn>())
        {
            CstStatReturn* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatReturn>();

            writer.keyword("return");

            CommaSeparatorInserter comma(writer, cstNode ? cstNode->commaPositions.begin() : nullptr);
            for (const auto& expr : a->list)
            {
                comma();
                visualize(*expr);
            }
        }
        else if (const auto& a = program.as<AstStatExpr>())
        {
            visualize(*a->expr);
        }
        else if (const auto& a = program.as<AstStatLocal>())
        {
            CstStatLocal* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatLocal>();

            writer.keyword("local");

            CommaSeparatorInserter varComma(writer, cstNode ? cstNode->varsCommaPositions.begin() : nullptr);
            for (const auto& local : a->vars)
            {
                varComma();
                visualize(*local);
            }

            if (a->equalsSignLocation)
            {
                advance(a->equalsSignLocation->begin);
                writer.symbol("=");
            }


            CommaSeparatorInserter valueComma(writer, cstNode ? cstNode->valuesCommaPositions.begin() : nullptr);
            for (const auto& value : a->values)
            {
                valueComma();
                visualize(*value);
            }
        }
        else if (const auto& a = program.as<AstStatFor>())
        {
            CstStatFor* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatFor>();

            writer.keyword("for");

            visualize(*a->var);
            if (cstNode)
                advance(cstNode->equalsPosition);
            writer.symbol("=");
            visualize(*a->from);
            if (cstNode)
                advance(cstNode->endCommaPosition);
            writer.symbol(",");
            visualize(*a->to);
            if (a->step)
            {
                if (cstNode && cstNode->stepCommaPosition)
                    advance(*cstNode->stepCommaPosition);
                writer.symbol(",");
                visualize(*a->step);
            }
            advance(a->doLocation.begin);
            writer.keyword("do");
            visualizeBlock(*a->body);

            writeEnd(program.location);
        }
        else if (const auto& a = program.as<AstStatForIn>())
        {
            CstStatForIn* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatForIn>();

            writer.keyword("for");

            CommaSeparatorInserter varComma(writer, cstNode ? cstNode->varsCommaPositions.begin() : nullptr);
            for (const auto& var : a->vars)
            {
                varComma();
                visualize(*var);
            }

            advance(a->inLocation.begin);
            writer.keyword("in");

            CommaSeparatorInserter valComma(writer, cstNode ? cstNode->valuesCommaPositions.begin() : nullptr);

            for (const auto& val : a->values)
            {
                valComma();
                visualize(*val);
            }

            advance(a->doLocation.begin);
            writer.keyword("do");

            visualizeBlock(*a->body);

            writeEnd(program.location);
        }
        else if (const auto& a = program.as<AstStatAssign>())
        {
            CstStatAssign* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatAssign>();

            CommaSeparatorInserter varComma(writer, cstNode ? cstNode->varsCommaPositions.begin() : nullptr);
            for (const auto& var : a->vars)
            {
                varComma();
                visualize(*var);
            }

            if (cstNode)
                advance(cstNode->equalsPosition);
            writer.symbol("=");

            CommaSeparatorInserter valueComma(writer, cstNode ? cstNode->valuesCommaPositions.begin() : nullptr);
            for (const auto& value : a->values)
            {
                valueComma();
                visualize(*value);
            }
        }
        else if (const auto& a = program.as<AstStatCompoundAssign>())
        {
            CstStatCompoundAssign* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatCompoundAssign>();

            visualize(*a->var);

            if (cstNode)
                advance(cstNode->opPosition);

            switch (a->op)
            {
            case AstExprBinary::Add:
                writer.symbol("+=");
                break;
            case AstExprBinary::Sub:
                writer.symbol("-=");
                break;
            case AstExprBinary::Mul:
                writer.symbol("*=");
                break;
            case AstExprBinary::Div:
                writer.symbol("/=");
                break;
            case AstExprBinary::FloorDiv:
                writer.symbol("//=");
                break;
            case AstExprBinary::Mod:
                writer.symbol("%=");
                break;
            case AstExprBinary::Pow:
                writer.symbol("^=");
                break;
            case AstExprBinary::Concat:
                writer.symbol("..=");
                break;
            default:
                LUAU_ASSERT(!"Unexpected compound assignment op");
            }

            visualize(*a->value);
        }
        else if (const auto& a = program.as<AstStatFunction>())
        {
            writer.keyword("function");
            visualize(*a->name);
            visualizeFunctionBody(*a->func);
        }
        else if (const auto& a = program.as<AstStatLocalFunction>())
        {
            CstStatLocalFunction* cstNode = nullptr;
            if (const auto& c = cstNodeMap[a])
                cstNode = c->as<CstStatLocalFunction>();

            writer.keyword("local");

            if (cstNode)
                advance(cstNode->functionKeywordPosition);
            else
                writer.space();

            writer.keyword("function");
            advance(a->name->location.begin);
            writer.identifier(a->name->name.value);
            visualizeFunctionBody(*a->func);
        }
        else if (const auto& a = program.as<AstStatTypeAlias>())
        {
            if (writeTypes)
            {
                if (a->exported)
                    writer.keyword("export");

                writer.keyword("type");
                writer.identifier(a->name.value);
                if (a->generics.size > 0 || a->genericPacks.size > 0)
                {
                    writer.symbol("<");
                    CommaSeparatorInserter comma(writer);

                    for (auto o : a->generics)
                    {
                        comma();

                        writer.advance(o.location.begin);
                        writer.identifier(o.name.value);

                        if (o.defaultValue)
                        {
                            writer.maybeSpace(o.defaultValue->location.begin, 2);
                            writer.symbol("=");
                            visualizeTypeAnnotation(*o.defaultValue);
                        }
                    }

                    for (auto o : a->genericPacks)
                    {
                        comma();

                        writer.advance(o.location.begin);
                        writer.identifier(o.name.value);
                        writer.symbol("...");

                        if (o.defaultValue)
                        {
                            writer.maybeSpace(o.defaultValue->location.begin, 2);
                            writer.symbol("=");
                            visualizeTypePackAnnotation(*o.defaultValue, false);
                        }
                    }

                    writer.symbol(">");
                }
                writer.maybeSpace(a->type->location.begin, 2);
                writer.symbol("=");
                visualizeTypeAnnotation(*a->type);
            }
        }
        else if (const auto& t = program.as<AstStatTypeFunction>())
        {
            if (writeTypes)
            {
                writer.keyword("type function");
                writer.identifier(t->name.value);
                visualizeFunctionBody(*t->body);
            }
        }
        else if (const auto& a = program.as<AstStatError>())
        {
            writer.symbol("(error-stat");

            for (size_t i = 0; i < a->expressions.size; i++)
            {
                writer.symbol(i == 0 ? ": " : ", ");
                visualize(*a->expressions.data[i]);
            }

            for (size_t i = 0; i < a->statements.size; i++)
            {
                writer.symbol(i == 0 && a->expressions.size == 0 ? ": " : ", ");
                visualize(*a->statements.data[i]);
            }

            writer.symbol(")");
        }
        else
        {
            LUAU_ASSERT(!"Unknown AstStat");
        }

        if (program.hasSemicolon)
            writer.symbol(";");
    }

    void visualizeFunctionBody(AstExprFunction& func)
    {
        if (func.generics.size > 0 || func.genericPacks.size > 0)
        {
            CommaSeparatorInserter comma(writer);
            writer.symbol("<");
            for (const auto& o : func.generics)
            {
                comma();

                writer.advance(o.location.begin);
                writer.identifier(o.name.value);
            }
            for (const auto& o : func.genericPacks)
            {
                comma();

                writer.advance(o.location.begin);
                writer.identifier(o.name.value);
                writer.symbol("...");
            }
            writer.symbol(">");
        }

        writer.symbol("(");
        CommaSeparatorInserter comma(writer);

        for (size_t i = 0; i < func.args.size; ++i)
        {
            AstLocal* local = func.args.data[i];

            comma();

            advance(local->location.begin);
            writer.identifier(local->name.value);
            if (writeTypes && local->annotation)
            {
                writer.symbol(":");
                visualizeTypeAnnotation(*local->annotation);
            }
        }

        if (func.vararg)
        {
            comma();
            advance(func.varargLocation.begin);
            writer.symbol("...");

            if (func.varargAnnotation)
            {
                writer.symbol(":");
                visualizeTypePackAnnotation(*func.varargAnnotation, true);
            }
        }

        writer.symbol(")");

        if (writeTypes && func.returnAnnotation)
        {
            writer.symbol(":");
            writer.space();

            visualizeTypeList(*func.returnAnnotation, false);
        }

        visualizeBlock(*func.body);
        writeEnd(func.location);
    }

    void visualizeBlock(AstStatBlock& block)
    {
        for (const auto& s : block.body)
            visualize(*s);
        writer.advance(block.location.end);
    }

    void visualizeBlock(AstStat& stat)
    {
        if (AstStatBlock* block = stat.as<AstStatBlock>())
            visualizeBlock(*block);
        else
            LUAU_ASSERT(!"visualizeBlock was expecting an AstStatBlock");
    }

    void visualizeElseIf(AstStatIf& elseif)
    {
        visualize(*elseif.condition);
        if (elseif.thenLocation)
            advance(elseif.thenLocation->begin);
        writer.keyword("then");
        visualizeBlock(*elseif.thenbody);

        if (elseif.elsebody == nullptr)
        {
            writeEnd(elseif.location);
        }
        else if (auto elseifelseif = elseif.elsebody->as<AstStatIf>())
        {
            if (elseif.elseLocation)
                advance(elseif.elseLocation->begin);
            writer.keyword("elseif");
            visualizeElseIf(*elseifelseif);
        }
        else
        {
            if (elseif.elseLocation)
                advance(elseif.elseLocation->begin);
            writer.keyword("else");

            visualizeBlock(*elseif.elsebody);
            writeEnd(elseif.location);
        }
    }

    void visualizeTypeAnnotation(const AstType& typeAnnotation)
    {
        advance(typeAnnotation.location.begin);
        if (const auto& a = typeAnnotation.as<AstTypeReference>())
        {
            if (a->prefix)
            {
                writer.write(a->prefix->value);
                writer.symbol(".");
            }

            writer.write(a->name.value);
            if (a->parameters.size > 0 || a->hasParameterList)
            {
                CommaSeparatorInserter comma(writer);
                writer.symbol("<");
                for (auto o : a->parameters)
                {
                    comma();

                    if (o.type)
                        visualizeTypeAnnotation(*o.type);
                    else
                        visualizeTypePackAnnotation(*o.typePack, false);
                }

                writer.symbol(">");
            }
        }
        else if (const auto& a = typeAnnotation.as<AstTypeFunction>())
        {
            if (a->generics.size > 0 || a->genericPacks.size > 0)
            {
                CommaSeparatorInserter comma(writer);
                writer.symbol("<");
                for (const auto& o : a->generics)
                {
                    comma();

                    writer.advance(o.location.begin);
                    writer.identifier(o.name.value);
                }
                for (const auto& o : a->genericPacks)
                {
                    comma();

                    writer.advance(o.location.begin);
                    writer.identifier(o.name.value);
                    writer.symbol("...");
                }
                writer.symbol(">");
            }

            {
                visualizeTypeList(a->argTypes, true);
            }

            writer.symbol("->");
            visualizeTypeList(a->returnTypes, true);
        }
        else if (const auto& a = typeAnnotation.as<AstTypeTable>())
        {
            AstTypeReference* indexType = a->indexer ? a->indexer->indexType->as<AstTypeReference>() : nullptr;

            if (a->props.size == 0 && indexType && indexType->name == "number")
            {
                writer.symbol("{");
                visualizeTypeAnnotation(*a->indexer->resultType);
                writer.symbol("}");
            }
            else
            {
                CommaSeparatorInserter comma(writer);

                writer.symbol("{");

                for (std::size_t i = 0; i < a->props.size; ++i)
                {
                    comma();
                    advance(a->props.data[i].location.begin);
                    writer.identifier(a->props.data[i].name.value);
                    if (a->props.data[i].type)
                    {
                        writer.symbol(":");
                        visualizeTypeAnnotation(*a->props.data[i].type);
                    }
                }
                if (a->indexer)
                {
                    comma();
                    writer.symbol("[");
                    visualizeTypeAnnotation(*a->indexer->indexType);
                    writer.symbol("]");
                    writer.symbol(":");
                    visualizeTypeAnnotation(*a->indexer->resultType);
                }
                writer.symbol("}");
            }
        }
        else if (auto a = typeAnnotation.as<AstTypeTypeof>())
        {
            writer.keyword("typeof");
            writer.symbol("(");
            visualize(*a->expr);
            writer.symbol(")");
        }
        else if (const auto& a = typeAnnotation.as<AstTypeUnion>())
        {
            if (a->types.size == 2)
            {
                AstType* l = a->types.data[0];
                AstType* r = a->types.data[1];

                auto lta = l->as<AstTypeReference>();
                if (lta && lta->name == "nil")
                    std::swap(l, r);

                // it's still possible that we had a (T | U) or (T | nil) and not (nil | T)
                auto rta = r->as<AstTypeReference>();
                if (rta && rta->name == "nil")
                {
                    bool wrap = l->as<AstTypeIntersection>() || l->as<AstTypeFunction>();

                    if (wrap)
                        writer.symbol("(");

                    visualizeTypeAnnotation(*l);

                    if (wrap)
                        writer.symbol(")");

                    writer.symbol("?");
                    return;
                }
            }

            for (size_t i = 0; i < a->types.size; ++i)
            {
                if (i > 0)
                {
                    writer.maybeSpace(a->types.data[i]->location.begin, 2);
                    writer.symbol("|");
                }

                bool wrap = a->types.data[i]->as<AstTypeIntersection>() || a->types.data[i]->as<AstTypeFunction>();

                if (wrap)
                    writer.symbol("(");

                visualizeTypeAnnotation(*a->types.data[i]);

                if (wrap)
                    writer.symbol(")");
            }
        }
        else if (const auto& a = typeAnnotation.as<AstTypeIntersection>())
        {
            for (size_t i = 0; i < a->types.size; ++i)
            {
                if (i > 0)
                {
                    writer.maybeSpace(a->types.data[i]->location.begin, 2);
                    writer.symbol("&");
                }

                bool wrap = a->types.data[i]->as<AstTypeUnion>() || a->types.data[i]->as<AstTypeFunction>();

                if (wrap)
                    writer.symbol("(");

                visualizeTypeAnnotation(*a->types.data[i]);

                if (wrap)
                    writer.symbol(")");
            }
        }
        else if (const auto& a = typeAnnotation.as<AstTypeSingletonBool>())
        {
            writer.keyword(a->value ? "true" : "false");
        }
        else if (const auto& a = typeAnnotation.as<AstTypeSingletonString>())
        {
            writer.string(std::string_view(a->value.data, a->value.size));
        }
        else if (typeAnnotation.is<AstTypeError>())
        {
            writer.symbol("%error-type%");
        }
        else
        {
            LUAU_ASSERT(!"Unknown AstType");
        }
    }
};

std::string toString(AstNode* node)
{
    StringWriter writer;
    writer.pos = node->location.begin;

    Printer printer(writer, CstNodeMap{nullptr});
    printer.writeTypes = true;

    if (auto statNode = node->asStat())
        printer.visualize(*statNode);
    else if (auto exprNode = node->asExpr())
        printer.visualize(*exprNode);
    else if (auto typeNode = node->asType())
        printer.visualizeTypeAnnotation(*typeNode);

    return writer.str();
}

void dump(AstNode* node)
{
    printf("%s\n", toString(node).c_str());
}

std::string transpile(AstStatBlock& block, const CstNodeMap& cstNodeMap)
{
    StringWriter writer;
    Printer(writer, cstNodeMap).visualizeBlock(block);
    return writer.str();
}

std::string transpileWithTypes(AstStatBlock& block, const CstNodeMap& cstNodeMap)
{
    StringWriter writer;
    Printer printer(writer, cstNodeMap);
    printer.writeTypes = true;
    printer.visualizeBlock(block);
    return writer.str();
}

std::string transpileWithTypes(AstStatBlock& block)
{
    // TODO: remove this interface?
    return transpileWithTypes(block, CstNodeMap{nullptr});
}

TranspileResult transpile(std::string_view source, ParseOptions options, bool withTypes)
{
    auto allocator = Allocator{};
    auto names = AstNameTable{allocator};
    ParseResult parseResult = Parser::parse(source.data(), source.size(), names, allocator, options);

    if (!parseResult.errors.empty())
    {
        // TranspileResult keeps track of only a single error
        const ParseError& error = parseResult.errors.front();

        return TranspileResult{"", error.getLocation(), error.what()};
    }

    LUAU_ASSERT(parseResult.root);
    if (!parseResult.root)
        return TranspileResult{"", {}, "Internal error: Parser yielded empty parse tree"};

    if (withTypes)
        return TranspileResult{transpileWithTypes(*parseResult.root, parseResult.cstNodeMap)};

    return TranspileResult{transpile(*parseResult.root, parseResult.cstNodeMap)};
}

} // namespace Luau
