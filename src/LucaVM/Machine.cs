using Luca.Parser;
using Luca.Parser.Ast;

namespace Luca;

public class MachineRuntimeError : Exception { }
public class NameCollisionError : MachineRuntimeError { }
public class IllformedProgramError : MachineRuntimeError { }
public class IdNotFoundError : MachineRuntimeError { }

public sealed class Machine
{
    private readonly Dictionary<string, IExpr> _namedExprLookup = new();

    public IEnumerable<IExpr> Digest(string source)
    {
        var parser = new OneTimeParser(source);
        foreach (var stmt in parser.ParseProgram())
        {
            if (stmt is NamedStmt nstmt)
            {
                if (!_namedExprLookup.ContainsKey(nstmt.Id.Name))
                {
                    var res = Evaluate(nstmt.Value);
                    _namedExprLookup.Add(nstmt.Id.Name, res);
                    yield return res;
                }
                else
                {
                    throw new NameCollisionError();
                }
            }
            else if (stmt is ExprStmt estmt)
            {
                var res = Evaluate(estmt.Expr);
                yield return res;
            }
        }
    }
    public void Reset() { }

    private IExpr Evaluate(IExpr expr)
    {
        if (expr is EvalExpr evalExpr)
        {
            return Evaluate(evalExpr);
        }
        else if (expr is ArithmeticExpr mathExpr)
        {
            return Evaluate(mathExpr);
        }
        else if (expr is IdTerm id)
        {
            return Evaluate(id);
        }
        return expr;
    }
    private IExpr Evaluate(EvalExpr evalExpr)
    {
        throw new NotImplementedException();
    }
    private IExpr Evaluate(ArithmeticExpr mathExpr)
    {
        if (mathExpr is BinaryOpExpr binaryExpr)
        {
            var left = Evaluate(binaryExpr.Left);
            var right = Evaluate(binaryExpr.Right);
            ExpectNode<IntValueTerm>(left);
            ExpectNode<IntValueTerm>(right);

            var leftVal = ((IntValueTerm)left).Value;
            var rightVal = ((IntValueTerm)right).Value;

            switch (binaryExpr.Operator)
            {
                case OperatorPlus:
                    return new IntValueTerm { Value = leftVal + rightVal };
                case OperatorMinus:
                    return new IntValueTerm { Value = leftVal - rightVal };
                case OperatorMultiply:
                    return new IntValueTerm { Value = leftVal * rightVal };
                case OperatorDivide:
                    return new IntValueTerm { Value = leftVal / rightVal };
                default:
                    throw new ArgumentException();
            }
        }
        else if (mathExpr is UnaryOpExpr unaryExpr)
        {
            var term = Evaluate(unaryExpr.Term);
            ExpectNode<IntValueTerm>(term);
            ExpectTrue(unaryExpr.Operator is OperatorMinus);
            return new IntValueTerm { Value = -((IntValueTerm)term).Value };
        }
        throw new ArgumentException();
    }
    private IExpr Evaluate(IdTerm id)
    {
        if (_namedExprLookup.TryGetValue(id.Id.Name, out var expr))
        {
            return expr;
        }
        else
        {
            throw new IdNotFoundError();
        }
    }

    private static void ExpectNode<T>(IExpr expr)
    {
        if (expr is not T)
        {
            throw new IllformedProgramError();
        }
    }
    private static void ExpectTrue(bool expr)
    {
        if (!expr) throw new ParseError();
    }
}
