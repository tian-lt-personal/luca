using Luca.Parser;

namespace Luca.UnitTesting;

public class ParserTests
{
    [Theory]
    [InlineData("x")]
    [InlineData("(x)")]
    [InlineData("x y")]
    [InlineData("x.x")]
    [InlineData("x.(x)")]
    [InlineData("(x.x)")]
    [InlineData("y.x.x y")]
    [InlineData("(y.x.x) y")]
    [InlineData("x y z")]
    [InlineData("(x y z)")]
    [InlineData("(x y) z")]
    [InlineData("x (y z)")]
    [InlineData("(x (y z))")]
    public void BasicExpr(string source)
    {
        var parser = new OneTimeParser(source);
        var tree = parser.RunPass();
    }

    [Theory]
    [InlineData("(")]
    [InlineData(")")]
    [InlineData("(x")]
    [InlineData("x)")]
    [InlineData("x.")]
    [InlineData(".x")]
    [InlineData(".x.")]
    [InlineData("(.x.")]
    [InlineData(".x.)")]
    [InlineData("x.y.")]
    [InlineData(".x.y.")]
    public void BadExpr(string source)
    {
        try
        {
            var parser = new OneTimeParser(source);
            parser.RunPass();
        }
        catch (ParseError)
        {
            return;
        }
        Assert.Fail("Expected ParseError");
    }
}
