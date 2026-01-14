namespace Luca.Parser.Ast;

public interface IExpr { }

public class AbstractionExpr : IExpr
{
    public required Identifier Var { get; init; }
    public required IExpr Def { get; init; }
}

public class ApplicationExpr : IExpr
{

}

public class ArithmeticExpr : IExpr
{ }

