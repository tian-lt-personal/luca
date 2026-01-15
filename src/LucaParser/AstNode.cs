namespace Luca.Parser.Ast;

public interface IExpr { }

public class Term : IExpr
{
}

public class IdTerm : Term
{
    public required Identifier Id { get; init; }
}
public class ValueTerm : Term
{
    public required IntegerLiteral Value { get; init; }
}
public class ParenTerm : Term
{
    public required IExpr InnerExpr { get; init; }
}

public class FunctionExpr : IExpr
{
    public required Identifier Var { get; init; }
    public required IExpr Def { get; init; }
}

public class EvalExpr : IExpr
{
    public required IExpr Functor { get; init; }
    public required IExpr Argument { get; init; }
}

public class ArithmeticExpr : IExpr
{ }
