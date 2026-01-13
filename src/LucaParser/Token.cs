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

public class OperatorPropOf : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorPlus : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorMinus : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorMultiply : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorDivide : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorRem : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorEq : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorNe : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorLeftParen : IToken
{
    public TokenType Type => TokenType.Operator;
}
public class OperatorRightParen : IToken
{
    public TokenType Type => TokenType.Operator;
}
