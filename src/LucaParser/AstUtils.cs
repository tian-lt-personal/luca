using System.Text;

namespace Luca.Parser.Ast;

public static class AstUtils
{
    public static string Dump(IExpr node, int level = 0)
    {
        var builder = new StringBuilder();

        if (node is IdTerm id)
        {
            builder.Append($"[id: {id.Id.Name}]");
        }
        else if (node is ValueTerm val)
        {
            builder.Append(Dump(val));
        }
        else if (node is ParenTerm paren)
        {
            //builder.Append($"[paren:\n");
            //AppendMargin(builder, level + 1);
            //builder.Append($"{Dump(paren.InnerExpr, level + 1)}\n");
            //AppendMargin(builder, level);
            //builder.Append(" -- paren]");

            builder.Append($"[paren: {Dump(paren.InnerExpr, level + 1)}]");
        }
        else if (node is EvalExpr eval)
        {
            builder.Append($"[eval:\n");
            AppendMargin(builder, level + 1);
            builder.Append($".functor = {Dump(eval.Functor, level + 1)}\n");
            AppendMargin(builder, level + 1);
            builder.Append($".argument = {Dump(eval.Argument, level + 1)}\n");
            AppendMargin(builder, level);
            builder.Append(" -- eval]");
        }
        else if (node is FunctionTerm func)
        {
            builder.Append($"[func:\n");
            AppendMargin(builder, level + 1);
            builder.Append($".var = {func.Var.Name}\n");
            AppendMargin(builder, level + 1);
            builder.Append($".def = {Dump(func.Def, level + 1)}\n");
            AppendMargin(builder, level);
            builder.Append(" -- func]");
        }
        else if (node is UnaryOpExpr uop)
        {
            builder.Append($"[op: {DumpOp(uop.Operator)} {Dump(uop.Term, level + 1)}]");
        }
        else if (node is BinaryOpExpr bop)
        {
            builder.Append($"[op: {Dump(bop.Left, level + 1)} {DumpOp(bop.Operator)} {Dump(bop.Right, level + 1)}]");
        }
        return builder.ToString();
    }

    public static string Dump(ValueTerm term)
    {
        if (term.Value is IntegerLiteral intVal)
        {
            return $"[value-int: {intVal.Value}]";
        }
        return "!!! error value term !!!";
    }

    public static string DumpOp(IToken op)
    {
        if (op is OperatorPlus)
        {
            return "+";
        }
        else if (op is OperatorMinus)
        {
            return "-";
        }
        else if (op is OperatorMultiply)
        {
            return "*";
        }
        else if (op is OperatorDivide)
        {
            return "/";
        }
        else if (op is OperatorRem)
        {
            return "%";
        }
        return "!!! error operator token !!!";
    }

    private static void AppendMargin(StringBuilder builder, int level)
    {
        for (int i = 0; i < level; i++)
        {
            builder.Append("  ");
        }
    }
}
