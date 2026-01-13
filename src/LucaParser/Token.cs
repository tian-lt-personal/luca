namespace Luca.Parser;

public interface IToken
{
    public TokenType Type { get; }
}

public class IntegerLiteral : IToken
{
    public TokenType Type => TokenType.IntegerLiteral;
    public int Value { get; init; }

    public IntegerLiteral(string value)
    {
        Value = int.Parse(value);
    }
}

public class KeywordIf : IToken
{
    public TokenType Type => TokenType.Keyword;
}
public class KeywordThen : IToken
{
    public TokenType Type => TokenType.Keyword;
}
public class KeywordElse : IToken
{
    public TokenType Type => TokenType.Keyword;
}
public class KeywordCurexpr : IToken
{
    public TokenType Type => TokenType.Keyword;
}
