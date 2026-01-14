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
            return ParseAbstraction();
        }

        throw new NotImplementedException();
    }

    private AbstractionExpr ParseAbstraction()
    {
        // expect identifier
        ExpectToken<Identifier>(_tok);
        var param = (Identifier)_tok;
        MoveNext();
        // expect dot
        ExpectToken<OperatorDot>(_tok);
        MoveNext();
        var body = ParseExpr();
        return new AbstractionExpr { Var = param, Def = body };
    }

    private IToken MoveNext()
    {
        _tok = _tokNext!;
        _tokNext = _lex.TryGetToken();
        return _tok;
    }

    private static void ExpectToken<T>(IToken token) where T : IToken
    {
        if (token is not T) throw new ParseError();
    }
}
