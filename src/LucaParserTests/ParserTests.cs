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
    public void SimpleAbstractions(string source)
    {
        var parser = new OneTimeParser(source);
        var tree = parser.RunPass();
    }
}
