namespace Luca.Parser;

public interface IToken { }

public abstract class LiteralToken : IToken { }
public class IntegerLiteral : LiteralToken
{
    public int Value { get; init; }

    public IntegerLiteral(string value)
    {
        Value = int.Parse(value);
    }
}
public class BooleanLiteral : LiteralToken
{
    public bool Value { get; init; }
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
public class KeywordLet : KeywordToken { }
public class KeywordIf : KeywordToken { }
public class KeywordThen : KeywordToken { }
public class KeywordElse : KeywordToken { }
public class KeywordCurexpr : KeywordToken { }

public abstract class OperatorToken : IToken { }
public class OperatorPropOf : OperatorToken { }
public class OperatorDot : OperatorToken { }
public class OperatorPlus : OperatorToken { }
public class OperatorMinus : OperatorToken { }
public class OperatorMultiply : OperatorToken { }
public class OperatorDivide : OperatorToken { }
public class OperatorRem : OperatorToken { }
public class OperatorEq : OperatorToken { }
public class OperatorNe : OperatorToken { }
public class OperatorGt : OperatorToken { }
public class OperatorLt : OperatorToken { }
public class OperatorAnd : OperatorToken { }
public class OperatorOr : OperatorToken { }
public class OperatorLeftParen : OperatorToken { }
public class OperatorRightParen : OperatorToken { }
public class OperatorSemicolon : OperatorToken { }
public class OperatorLeftBrace : OperatorToken { }
public class OperatorRightBrace : OperatorToken { }
