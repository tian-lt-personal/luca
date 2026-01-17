using Luca.Parser.Ast;

namespace Luca.Parser;

public class ParseError : Exception { }

public sealed class OneTimeParser
{
    private readonly Lexer _lex;
    private IToken _tok;
    private IToken? _tokNext;

    public OneTimeParser(string source)
    {
        _lex = new Lexer(source);
        _tok = _lex.GetToken();
        _tokNext = _lex.TryGetToken();
    }

    public IExpr RunPass()
    {
        var expr = ParseExpr();
        if (_tok != null)
        {
            throw new ParseError();
        }
        return expr;
    }

    private IExpr ParseExpr()
    {
        IExpr left;
        if (IsUnaryArithmeticOperator(_tok))
        {
            left = ParseArithmetic(null);
        }
        else
        {
            var term = ParseTerm();
            if (IsBinaryArithmeticOperator(_tok))
            {
                left = ParseArithmetic(term);
            }
            else
            {
                left = term;
            }
        }

        do
        {
            if (_tok is OperatorRightParen)
            {
                return left;
            }

            IExpr? right;
            if (IsUnaryArithmeticOperator(_tok))
            {
                right = ParseArithmetic(null);
            }
            else
            {
                var term = TryParseTerm();
                if (term == null)
                {
                    return left;
                }
                if (IsBinaryArithmeticOperator(_tok))
                {
                    right = ParseArithmetic(term);
                }
                else
                {
                    right = term;
                }
            }

            left = new EvalExpr { Functor = left, Argument = right };
        }
        while (true);
    }

    private IExpr ParseArithmetic(Term? beginning)
    {
        if (_tok is OperatorMinus op)
        { // unary minus
            MoveNext();
            var term = ParseTerm();
            return new UnaryOpExpr { Operator = op, Term = term! };
        }
        else
        {
            var term = ParseTerm();
        }
        throw new NotImplementedException();
    }

    private Term ParseTerm()
    {
        var term = TryParseTerm();
        ExpectTrue(term != null);
        return term!;
    }

    private Term? TryParseTerm()
    {

        if (_tok is Identifier id)
        {
            if (_tokNext is OperatorDot)
            {
                return ParseFunc();
            }
            MoveNext();
            return new IdTerm { Id = id };
        }
        else if (_tok is IntegerLiteral intVal)
        {
            MoveNext();
            return new ValueTerm { Value = intVal };
        }
        else if (_tok is OperatorLeftParen)
        {
            MoveNext();
            var expr = ParseExpr();
            ExpectToken<OperatorRightParen>(_tok);
            MoveNext();
            return new ParenTerm { InnerExpr = expr };
        }
        return null;
    }

    private FunctionTerm ParseFunc()
    {
        // expect identifier
        ExpectToken<Identifier>(_tok);
        var param = (Identifier)_tok;
        MoveNext();

        // expect dot
        ExpectToken<OperatorDot>(_tok);
        MoveNext();
        var body = ParseExpr();
        //MoveNext();
        return new FunctionTerm { Var = param, Def = body };
    }

    private IToken MoveNext()
    {
        _tok = _tokNext!;
        _tokNext = _lex.TryGetToken();
        return _tok;
    }

    private static bool IsUnaryArithmeticOperator(IToken token)
    {
        return token is OperatorMinus or OperatorPlus;
    }

    private static bool IsBinaryArithmeticOperator(IToken token)
    {
        return token is OperatorPlus or OperatorMinus or OperatorMultiply or OperatorDivide;
    }

    private static void ExpectToken<T>(IToken token) where T : IToken
    {
        if (token is not T) throw new ParseError();
    }

    private static void ExpectTrue(bool expr)
    {
        if (!expr) throw new ParseError();
    }
}
