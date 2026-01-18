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
            left = ParseArithmeticExpr(null, 0);
        }
        else
        {
            var term = ParseTerm();
            if (IsBinaryArithmeticOperator(_tok))
            {
                left = ParseArithmeticExpr(term, 0);
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
                right = ParseArithmeticExpr(null, 0);
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
                    right = ParseArithmeticExpr(term, 0);
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

    private IExpr ParseArithmeticExpr(Term? beginning, int minPrecedence)
    {
        IExpr left;
        if (beginning == null)
        {
            if (_tok is OperatorMinus op)
            { // unary minus
                MoveNext();
                var term = ParseTerm();
                left = new UnaryOpExpr { Operator = op, Term = term! };
            }
            else
            {
                left = ParseTerm();
            }
        }
        else
        {
            left = beginning;
        }

        while (IsBinaryArithmeticOperator(_tok) && GetPrecedence(_tok) > minPrecedence)
        {
            var op = _tok;
            MoveNext();
            var right = ParseArithmeticExpr(null, GetPrecedence(op));
            left = new BinaryOpExpr { Operator = op, Left = left, Right = right };
        }
        return left;
    }

    private Term ParseTerm()
    {
        var term = TryParseTerm();
        ExpectTrue(term != null);
        return term!;
    }

    private Term? TryParseTerm()
    {

        if (_tok is IdentifierToken id)
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
        ExpectToken<IdentifierToken>(_tok);
        var param = (IdentifierToken)_tok;
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

    private static int GetPrecedence(IToken token)
    {
        return token switch
        {
            OperatorPlus or OperatorMinus => 10,
            OperatorMultiply or OperatorDivide => 20,
            _ => throw new ParseError(),
        };
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
