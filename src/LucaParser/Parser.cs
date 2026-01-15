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
        if (_tokNext != null)
        {
            throw new ParseError();
        }
        return expr;
    }

    private IExpr ParseExpr()
    {
        if (_tok.Type == TokenType.Identifier && _tokNext is OperatorDot)
        {
            return ParseFunc();
        }
        return ParseEval();
    }

    private FunctionExpr ParseFunc()
    {
        // expect identifier
        ExpectToken<Identifier>(_tok);
        var param = (Identifier)_tok;
        MoveNext();

        // expect dot
        ExpectToken<OperatorDot>(_tok);
        MoveNext();
        var body = ParseExpr();
        MoveNext();
        return new FunctionExpr { Var = param, Def = body };
    }

    private IExpr ParseEval()
    {
        if (_tok is OperatorMinus)
        { // unary minus
        }

        do
        {
            var term = ParseTerm();
            if (IsArithmeticOperator(_tok))
            {
                ParseArithmetic();
            }
        }
        while (true);

        //return term;
    }

    private Term ParseTerm()
    {
        if (_tok is Identifier id)
        {
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
            var expr = ParseExpr();
            ExpectToken<OperatorRightParen>(_tok);
            MoveNext();
            return new ParenTerm { InnerExpr = expr };
        }
        throw new ParseError();
    }

    private IExpr ParseArithmetic()
    {
        throw new NotImplementedException();
    }

    private IToken MoveNext()
    {
        _tok = _tokNext!;
        _tokNext = _lex.TryGetToken();
        return _tok;
    }

    private static bool IsArithmeticOperator(IToken token)
    {
        return token is OperatorPlus or OperatorMinus or OperatorMultiply or OperatorDivide;
    }

    private static void ExpectToken<T>(IToken token) where T : IToken
    {
        if (token is not T) throw new ParseError();
    }
}
