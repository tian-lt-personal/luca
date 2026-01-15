using Luca.Parser;

namespace Luca.UnitTesting;

public class ParserTests
{
    [Theory]
    [InlineData("x.x")]
    [InlineData("y.x.x y")]
    public void SimpleAbstractions(string source)
    {
        var parser = new OneTimeParser(source);
        parser.RunPass();
    }
}
