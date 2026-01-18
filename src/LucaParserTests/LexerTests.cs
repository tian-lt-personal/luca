using Luca.Parser;

namespace Luca.UnitTesting;

public class LexerTests
{
    [Theory]
    [InlineData("")]
    [InlineData("?")]
    [InlineData(" ")]
    [InlineData("  ")]
    [InlineData("\n")]
    [InlineData("\r\n")]
    [InlineData("123b")]
    public void BadSource(string source)
    {
        bool failed = false;
        IToken? token = null;
        try
        {
            token = new Lexer(source).GetToken();
        }
        catch (LexerError)
        {
            failed = true;
        }
        if (!failed)
        {
            Assert.True(token is not KeywordToken);
        }
    }

    [Theory]
    [InlineData("a")]
    [InlineData("_")]
    [InlineData("abc_")]
    [InlineData("_abc")]
    [InlineData("a123")]
    [InlineData("a123b")]
    public void Identifier(string source)
    {
        var id = (IdentifierToken)new Lexer(source).GetToken();
        Assert.Equal(source, id.Name);
    }

    [Theory]
    [InlineData("if", nameof(KeywordIf))]
    [InlineData("then", nameof(KeywordThen))]
    [InlineData("else", nameof(KeywordElse))]
    [InlineData("curexpr", nameof(KeywordCurexpr))]
    public void Keywords(string source, string expectedType)
    {
        var token = new Lexer(source).GetToken();
        Assert.Equal(expectedType, token.GetType().Name);
    }

    [Theory]
    [InlineData(".", nameof(OperatorDot))]
    [InlineData("+", nameof(OperatorPlus))]
    [InlineData("-", nameof(OperatorMinus))]
    [InlineData("*", nameof(OperatorMultiply))]
    [InlineData("/", nameof(OperatorDivide))]
    [InlineData("%", nameof(OperatorRem))]
    [InlineData("(", nameof(OperatorLeftParen))]
    [InlineData(")", nameof(OperatorRightParen))]
    [InlineData("=", nameof(OperatorEq))]
    [InlineData("!=", nameof(OperatorNe))]
    [InlineData("->", nameof(OperatorPropOf))]
    public void Operators(string source, string expectedType)
    {
        var token = new Lexer(source).GetToken();
        Assert.Equal(expectedType, token.GetType().Name);
    }

    [Theory]
    [InlineData("0", 0)]
    [InlineData("1", 1)]
    [InlineData("2", 2)]
    [InlineData("100", 100)]
    [InlineData("001", 1)]
    [InlineData("12340", 12340)]
    [InlineData("01234", 1234)]
    public void IntegerLiterals(string source, int expectedValue)
    {
        var token = new Lexer(source).GetToken();
        Assert.Equal(expectedValue, ((IntegerLiteral)token).Value);
    }

    [Theory]
    [InlineData("if", nameof(KeywordIf))]
    [InlineData("if then", nameof(KeywordIf), nameof(KeywordThen))]
    [InlineData("if a then b", nameof(KeywordIf), nameof(IdentifierToken), nameof(KeywordThen), nameof(IdentifierToken))]
    [InlineData("if then else", nameof(KeywordIf), nameof(KeywordThen), nameof(KeywordElse))]
    [InlineData("1 else", nameof(IntegerLiteral), nameof(KeywordElse))]
    [InlineData("curexpr 001 if 3", nameof(KeywordCurexpr), nameof(IntegerLiteral), nameof(KeywordIf), nameof(IntegerLiteral))]
    [InlineData("    if  321  \n\r 20\n   if   \n", nameof(KeywordIf), nameof(IntegerLiteral), nameof(IntegerLiteral), nameof(KeywordIf))]
    [InlineData("if(1) then (2+3)\r\nelse 5!=2",
        nameof(KeywordIf), nameof(OperatorLeftParen), nameof(IntegerLiteral), nameof(OperatorRightParen),
        nameof(KeywordThen), nameof(OperatorLeftParen), nameof(IntegerLiteral), nameof(OperatorPlus),
        nameof(IntegerLiteral), nameof(OperatorRightParen), nameof(KeywordElse), nameof(IntegerLiteral),
        nameof(OperatorNe), nameof(IntegerLiteral))]
    public void TokenSequence(string source, params string[] expectedTypes)
    {
        var lexer = new Lexer(source);
        foreach (var expected in expectedTypes)
        {
            var token = lexer.GetToken();
            Assert.Equal(expected, token.GetType().Name);
        }
        Assert.True(HasDrained(lexer));
    }

    private static bool HasDrained(Lexer lexer)
    {
        try
        {
            lexer.GetToken();
        }
        catch (DrainedError)
        {
            return true;
        }
        return false;
    }
}
