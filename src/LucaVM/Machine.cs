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
    public required string Var { get; init; }
    public required IExpr Body { get; init; }
    public required Env Env { get; init; }
}

public sealed class Machine
{
    private readonly Stack<Env> _envs = new();

    public Machine()
    {
        _envs.Push(new Env());
    }

    public IEnumerable<IExpr> Digest(string source)
    {
        var parser = new OneTimeParser(source);
        foreach (var stmt in parser.ParseProgram())
        {
            if (stmt is LetStmt lstmt)
            {
                var res = Trampoline.Run(Evaluate(lstmt.Value, _envs.Peek()));
                _envs.Peek().Define(lstmt.Id, res);
                yield return res;
            }
            else if (stmt is ExprStmt estmt)
            {
                var res = Trampoline.Run(Evaluate(estmt.Expr, _envs.Peek()));
                yield return res;
            }
            else if (stmt is ScopeBeginStmt)
            {
                _envs.Push(new Env(_envs.Peek()));
            }
            else if (stmt is ScopeEndStmt)
            {
                ExpectTrue(_envs.Count > 0);
                _envs.Pop();
            }
            else
            {
                throw new NotImplementedException();
            }
        }
    }

    public static string Dump(IExpr expr)
    {
        if (expr is Closure f)
        {
            var res = "#closure \n";
            res += $".var = {f.Var}\n";
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
            IdTerm id => env.Lookup(id.Id),
            FunctionTerm func => new Closure
            {
                Var = func.Var,
                Body = func.Def,
                Env = env
            },
            ConditionExpr ifExpr => await Evaluate(ifExpr, env),
            _ => throw new NotImplementedException()
        };
    }
    private async LazyTask<IExpr> Evaluate(IExpr functor, IExpr argument)
    {
        ExpectNode<Closure>(functor);
        var f = (Closure)functor;
        var env = new Env(f.Env);
        env.Define(f.Var, argument);
        return await Evaluate(f.Body, env);
    }
    private async LazyTask<IExpr> Evaluate(ArithmeticExpr mathExpr, Env env)
    {
        if (mathExpr is BinaryOpExpr binaryExpr)
        {
            switch (binaryExpr.Operator)
            {
                case OperatorPlus:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new IntValueTerm { Value = ((IntValueTerm)left).Value + ((IntValueTerm)right).Value };
                    }
                case OperatorMinus:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new IntValueTerm { Value = ((IntValueTerm)left).Value - ((IntValueTerm)right).Value };
                    }
                case OperatorMultiply:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new IntValueTerm { Value = ((IntValueTerm)left).Value * ((IntValueTerm)right).Value };
                    }
                case OperatorDivide:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new IntValueTerm { Value = ((IntValueTerm)left).Value / ((IntValueTerm)right).Value };
                    }
                case OperatorGt:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new BoolValueTerm { Value = ((IntValueTerm)left).Value > ((IntValueTerm)right).Value };
                    }
                case OperatorLt:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<IntValueTerm>(left);
                        ExpectNode<IntValueTerm>(right);
                        return new BoolValueTerm { Value = ((IntValueTerm)left).Value < ((IntValueTerm)right).Value };
                    }
                case OperatorAnd:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        ExpectNode<BoolValueTerm>(left);
                        var valLeft = (BoolValueTerm)left;
                        if (!valLeft.Value)
                        {
                            return new BoolValueTerm { Value = false };
                        }
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<BoolValueTerm>(right);
                        var valRight = (BoolValueTerm)right;
                        return new BoolValueTerm { Value = valRight.Value };
                    }
                case OperatorOr:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        ExpectNode<BoolValueTerm>(left);
                        var valLeft = (BoolValueTerm)left;
                        if (valLeft.Value)
                        {
                            return new BoolValueTerm { Value = true };
                        }
                        var right = await Evaluate(binaryExpr.Right, env);
                        ExpectNode<BoolValueTerm>(right);
                        var valRight = (BoolValueTerm)right;
                        return new BoolValueTerm { Value = valRight.Value };
                    }
                case OperatorEq:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        if (left is BoolValueTerm bLeft)
                        {
                            var right = await Evaluate(binaryExpr.Right, env);
                            ExpectNode<BoolValueTerm>(right);
                            return new BoolValueTerm { Value = bLeft.Value == ((BoolValueTerm)right).Value };
                        }
                        else if (left is IntValueTerm iLeft)
                        {
                            var right = await Evaluate(binaryExpr.Right, env);
                            ExpectNode<IntValueTerm>(right);
                            return new BoolValueTerm { Value = iLeft.Value == ((IntValueTerm)right).Value };
                        }
                        else
                        {
                            throw new IllformedProgramError();
                        }
                    }
                case OperatorNe:
                    {
                        var left = await Evaluate(binaryExpr.Left, env);
                        if (left is BoolValueTerm bLeft)
                        {
                            var right = await Evaluate(binaryExpr.Right, env);
                            ExpectNode<BoolValueTerm>(right);
                            return new BoolValueTerm { Value = bLeft.Value != ((BoolValueTerm)right).Value };
                        }
                        else if (left is IntValueTerm iLeft)
                        {
                            var right = await Evaluate(binaryExpr.Right, env);
                            ExpectNode<IntValueTerm>(right);
                            return new BoolValueTerm { Value = iLeft.Value != ((IntValueTerm)right).Value };
                        }
                        else
                        {
                            throw new IllformedProgramError();
                        }
                    }
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
    private async LazyTask<IExpr> Evaluate(ConditionExpr ifExpr, Env env)
    {
        var cond = await Evaluate(ifExpr.Condition, env);
        ExpectNode<BoolValueTerm>(cond);
        if (((BoolValueTerm)cond).Value)
        {
            return await Evaluate(ifExpr.PositiveBranch, env);
        }
        else
        {
            return await Evaluate(ifExpr.NegativeBranch, env);
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
        if (!expr) throw new IllformedProgramError();
    }
}
