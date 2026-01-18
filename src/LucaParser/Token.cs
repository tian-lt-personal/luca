namespace Luca.Parser;

public interface IToken
{
}

public abstract class LiteralToken : IToken { }
public class IntegerLiteral : LiteralToken
{
    public int Value { get; init; }

    public IntegerLiteral(string value)
    {
        Value = int.Parse(value);
    }
}

public class IdentifierToken : IToken
{
    public string Name { get; init; }
    public IdentifierToken(string name)
    {
        Name = name;
    }
}

public abstract class KeywordToken : IToken { }
public class KeywordIf : IToken { }
public class KeywordThen : IToken { }
public class KeywordElse : IToken { }
public class KeywordCurexpr : IToken { }

public abstract class OperatorToken : IToken { }
public class OperatorPropOf : IToken { }
public class OperatorDot : IToken { }
public class OperatorPlus : IToken { }
public class OperatorMinus : IToken { }
public class OperatorMultiply : IToken { }
public class OperatorDivide : IToken { }
public class OperatorRem : IToken { }
public class OperatorEq : IToken { }
public class OperatorNe : IToken { }
public class OperatorLeftParen : IToken { }
public class OperatorRightParen : IToken { }
