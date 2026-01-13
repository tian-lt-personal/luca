using Luca.Parser;

namespace Luca.UnitTesting;

public class LexerTests
{
    [Theory]
    [InlineData("")]
    [InlineData("?")]
    [InlineData("bad_keyword")]
    public void BadSource(string source)
    {
        bool failed = false;
        try
        {
            new Lexer(source).GetToken();
        }
        catch (LexerError)
        {
            failed = true;
        }
        Assert.True(failed);
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
}
