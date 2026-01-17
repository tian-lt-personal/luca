namespace Luca.Parser.Ast;

public interface IExpr { }

public class Term : IExpr { }

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
public class FunctionTerm : Term
{
    public required Identifier Var { get; init; }
    public required IExpr Def { get; init; }
}

public class EvalExpr : IExpr
{
    public required IExpr Functor { get; init; }
    public required IExpr Argument { get; init; }
}

public class ArithmeticExpr : IExpr { }
public class UnaryOpExpr : ArithmeticExpr
{
    public required IToken Operator { get; init; }
    public required Term Term { get; init; }
}
public class BinaryOpExpr : ArithmeticExpr
{

}
