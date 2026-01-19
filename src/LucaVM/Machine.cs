using Luca.Parser;
using Luca.Parser.Ast;

namespace Luca;

public class MachineRuntimeError : Exception { }

public sealed class Machine
{
    private readonly Dictionary<string, IExpr> _namedExprLookup = new();

    public void Digest(string source)
    {
        var parser = new OneTimeParser(source);
        foreach (var stmt in parser.ParseProgram())
        {
            if (stmt is NamedStmt nstmt)
            {
                if (!_namedExprLookup.ContainsKey(nstmt.Id.Name))
                {
                    _namedExprLookup.Add(nstmt.Id.Name, Evaluate(nstmt.Value));
                }
                else
                {
                    throw new MachineRuntimeError();
                }
            }
            else if (stmt is ExprStmt estmt)
            {
                Evaluate(estmt.Expr);
            }
        }
    }
    public void Reset() { }

    private IExpr Evaluate(IExpr expr)
    {
        return expr;
    }
}
