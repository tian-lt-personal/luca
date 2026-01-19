namespace Luca.Parser.Ast;

public interface IExpr { }

public abstract class Term : IExpr { }

public class IdTerm : Term
{
    public required IdentifierToken Id { get; init; }
}
public abstract class ValueTerm : Term { }
public class IntValueTerm : ValueTerm
{
    public required int Value { get; init; }
}
public class ParenTerm : Term
{
    public required IExpr InnerExpr { get; init; }
}
public class FunctionTerm : Term
{
    public required IdentifierToken Var { get; init; }
    public required IExpr Def { get; init; }
}

public class EvalExpr : IExpr
{
    public required IExpr Function { get; init; }
    public required IExpr Argument { get; init; }
}

public abstract class ArithmeticExpr : IExpr { }
public class UnaryOpExpr : ArithmeticExpr
{
    public required IToken Operator { get; init; }
    public required Term Term { get; init; }
}
public class BinaryOpExpr : ArithmeticExpr
{
    public required IToken Operator { get; init; }
    public required IExpr Left { get; init; }
    public required IExpr Right { get; init; }
}

public interface IStmt { }

public class NamedStmt : IStmt
{
    public required IdentifierToken Id { get; init; }
    public required IExpr Value { get; init; }
}

public class ExprStmt : IStmt
{
    public required IExpr Expr { get; init; }
}

public class TypeStmt : IStmt
{
}
