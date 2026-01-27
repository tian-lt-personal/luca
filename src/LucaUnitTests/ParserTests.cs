using Luca.Parser;
using Luca.Parser.Ast;

namespace Luca.UnitTesting;

public class ParserTests
{
    [Theory]
    [InlineData("x;")]
    [InlineData("(x);")]
    [InlineData("x y;")]
    [InlineData("x.x;")]
    [InlineData("x.(x);")]
    [InlineData("(x.x);")]
    [InlineData("y.x.x y;")]
    [InlineData("(y.x.x) y;")]
    [InlineData("x y z;")]
    [InlineData("(x y z);")]
    [InlineData("(x y) z;")]
    [InlineData("x (y z);")]
    [InlineData("(x (y z));")]
    public void BasicExpr(string source)
    {
        var parser = new OneTimeParser(source);
        var prog = parser.ParseProgram().ToArray();
    }

    [Theory]
    [InlineData("x")]
    //[InlineData("let;")]
    [InlineData("(;")]
    [InlineData(");")]
    [InlineData("(x;")]
    [InlineData("x);")]
    [InlineData("x.;")]
    [InlineData(".x;")]
    [InlineData(".x.;")]
    [InlineData("(.x.;")]
    [InlineData(".x.);")]
    [InlineData("x.y.;")]
    [InlineData(".x.y.;")]
    public void BadExpr(string source)
    {
        try
        {
            var parser = new OneTimeParser(source);
            parser.ParseProgram().ToArray();
        }
        catch (ParseError)
        {
            return;
        }
        Assert.Fail("Expected ParseError");
    }

    [Theory]
    [InlineData("1;")]
    [InlineData("1+2;")]
    [InlineData("x+y+z;")]
    [InlineData("x+y*z;")]
    [InlineData("x*y/z;")]
    [InlineData("x*y-z;")]
    [InlineData("(x+y*z)-(u+v);")]
    public void ArithmeticExpr(string source)
    {
        var parser = new OneTimeParser(source);
        var prog = parser.ParseProgram().ToArray();
    }

    [Theory]
    [InlineData("x;", 1)]
    [InlineData("a; a+b;", 2)]
    [InlineData("a;\n   a+b;", 2)]
    [InlineData("let a = 1; let c= a+b; c;", 3)]
    public void StmtSequence(string source, int seqLen)
    {

        var parser = new OneTimeParser(source);
        var prog = parser.ParseProgram().ToArray();
        Assert.Equal(seqLen, prog.Length);
    }
}
