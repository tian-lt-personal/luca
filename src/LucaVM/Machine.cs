using Luca.Parser;
using Luca.Parser.Ast;

namespace Luca;

public class MachineRuntimeError : Exception
{
    public MachineRuntimeError() { }
    public MachineRuntimeError(string message) : base(message) { }
}
public class NameCollisionError : MachineRuntimeError
{
    public NameCollisionError(string name) : base($"name '{name}' has a conflict.") { }
}
public class IllformedProgramError : MachineRuntimeError { }
public class IdNotFoundError : MachineRuntimeError
{
    public IdNotFoundError(string idName) : base($"id '{idName}' not found.") { }
}

internal sealed class Env
{
    private readonly Dictionary<string, IExpr> _context = new();
    private readonly Env? _parent;

    public Env()
    {
        _parent = null;
    }

    public Env(Env parent)
    {
        _parent = parent;
    }

    public void Define(string name, IExpr expr)
    {
        if (!_context.ContainsKey(name))
        {
            _context.Add(name, expr);
        }
        else
        {
            throw new NameCollisionError(name);
        }
    }

    public IExpr Lookup(string name)
    {
        if (_context.TryGetValue(name, out var expr)) { return expr; }
        if (_parent != null) { return _parent.Lookup(name); }
        throw new IdNotFoundError(name);
    }
}

internal sealed class Closure : Term
{
    public required string VarName { get; init; }
    public required IExpr Body { get; init; }
    public required Env Env { get; init; }
}

public sealed class Machine
{
    private readonly Env _topEnv = new Env();

    public IEnumerable<IExpr> Digest(string source)
    {
        var parser = new OneTimeParser(source);
        foreach (var stmt in parser.ParseProgram())
        {
            if (stmt is NamedStmt nstmt)
            {
                var res = Trampoline.Run(Evaluate(nstmt.Value, _topEnv));
                _topEnv.Define(nstmt.Id.Name, res);
                yield return res;
            }
            else if (stmt is ExprStmt estmt)
            {
                var res = Trampoline.Run(Evaluate(estmt.Expr, _topEnv));
                yield return res;
            }
        }
    }

    public static string Dump(IExpr expr)
    {
        if (expr is Closure f)
        {
            var res = "#closure \n";
            res += $".varname = {f.VarName}\n";
            res += $".body = {Dump(f.Body)}\n";
            res += $".env = {f.Env.GetHashCode()}\n";
            res += "closure#";
            return res;
        }
        else
        {
            return AstUtils.Dump(expr);
        }
    }

    private async LazyTask<IExpr> Evaluate(IExpr expr, Env env)
    {
        return expr switch
        {
            EvalExpr evalExpr => await Evaluate(await Evaluate(evalExpr.Function, env), await Evaluate(evalExpr.Argument, env)),
            ArithmeticExpr mathExpr => await Evaluate(mathExpr, env),
            ValueTerm val => val,
            ParenTerm paren => await Evaluate(paren.InnerExpr, env),
            IdTerm id => env.Lookup(id.Id.Name),
            FunctionTerm func => new Closure
            {
                VarName = func.Var.Name,
                Body = func.Def,
                Env = env
            },
            _ => throw new NotImplementedException()
        };
    }
    private async LazyTask<IExpr> Evaluate(IExpr functor, IExpr argument)
    {
        ExpectNode<Closure>(functor);
        var f = (Closure)functor;
        var env = new Env(f.Env);
        env.Define(f.VarName, argument);
        return await Evaluate(f.Body, env);
    }
    private async LazyTask<IExpr> Evaluate(ArithmeticExpr mathExpr, Env env)
    {
        if (mathExpr is BinaryOpExpr binaryExpr)
        {
            var left = await Evaluate(binaryExpr.Left, env);
            var right = await Evaluate(binaryExpr.Right, env);
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
                    throw new NotImplementedException();
            }
        }
        else if (mathExpr is UnaryOpExpr unaryExpr)
        {
            var term = await Evaluate(unaryExpr.Term, env);
            ExpectNode<IntValueTerm>(term);
            ExpectTrue(unaryExpr.Operator is OperatorMinus);
            return new IntValueTerm { Value = -((IntValueTerm)term).Value };
        }
        throw new ArgumentException();
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
