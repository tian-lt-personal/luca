using Luca.Parser.Ast;

namespace Luca.Parser;

public class ParseError : Exception { }

public sealed class OneTimeParser
{
    private readonly Lexer _lex;
    private IToken _tok;
    private IToken? _tokNext;
    private int _scopeLevel = 0;

    public OneTimeParser(string source)
    {
        _lex = new Lexer(source);
        _tok = _lex.GetToken();
        _tokNext = _lex.TryGetToken();
    }

    public IEnumerable<IStmt> ParseProgram()
    {
        while (_tok != null)
        {
            if (_tok is KeywordLet)
            {
                ConsumeToken();
                ExpectToken<IdentifierToken>(_tok);
                var id = (IdentifierToken)_tok;
                ConsumeToken();
                ExpectToken<OperatorEq>(_tok);
                ConsumeToken();
                var expr = ParseExpr();
                ExpectToken<OperatorSemicolon>(_tok);
                ConsumeToken();
                yield return new LetStmt { Id = id.Name, Value = expr };
            }
            else if (_tok is OperatorLeftBrace)
            {
                ConsumeToken();
                ++_scopeLevel;
                yield return new ScopeBeginStmt();
            }
            else if (_tok is OperatorRightBrace)
            {
                ConsumeToken();
                ExpectTrue(_scopeLevel > 0);
                --_scopeLevel;
                yield return new ScopeEndStmt();
            }
            else if (_tok is OperatorSemicolon) { ConsumeToken(); }
            else
            {
                var expr = ParseExpr();
                ExpectToken<OperatorSemicolon>(_tok);
                ConsumeToken();
                yield return new ExprStmt { Expr = expr };
            }
        }
    }

    private IExpr ParseExpr()
    {
        IExpr left;
        if (IsUnaryArithmeticOperator(_tok))
        {
            left = ParseArithmeticExpr(null, 0);
        }
        else
        {
            IExpr term;
            if (_tok is KeywordIf)
            {
                term = ParseConditionExpr();
            }
            else
            {
                term = ParseTerm();
            }

            if (IsBinaryArithmeticOperator(_tok))
            {
                left = ParseArithmeticExpr(term, 0);
            }
            else
            {
                left = term;
            }
        }

        while (_tok is not
                OperatorRightParen or
                OperatorSemicolon or
                KeywordElse or
                KeywordThen)
        {
            IExpr? right;
            if (IsUnaryArithmeticOperator(_tok))
            {
                right = ParseArithmeticExpr(null, 0);
            }
            else
            {
                var term = TryParseTerm();
                if (term == null)
                {
                    return left;
                }
                if (IsBinaryArithmeticOperator(_tok))
                {
                    right = ParseArithmeticExpr(term, 0);
                }
                else
                {
                    right = term;
                }
            }

            left = new EvalExpr { Function = left, Argument = right };
        }
        return left;
    }

    private ConditionExpr ParseConditionExpr()
    {
        ExpectToken<KeywordIf>(_tok);
        ConsumeToken();

        var condExpr = ParseExpr();
        ExpectToken<KeywordThen>(_tok);
        ConsumeToken();

        var trueExpr = ParseExpr();
        ExpectToken<KeywordElse>(_tok);
        ConsumeToken();

        var falseExpr = ParseExpr();
        return new ConditionExpr
        {
            Condition = condExpr,
            PositiveBranch = trueExpr,
            NegativeBranch = falseExpr
        };
    }

    private IExpr ParseArithmeticExpr(IExpr? beginning, int minPrecedence)
    {
        IExpr left;
        if (beginning == null)
        {
            if (_tok is OperatorMinus op)
            { // unary minus
                ConsumeToken();
                var term = ParseTerm();
                left = new UnaryOpExpr { Operator = op, Term = term! };
            }
            else
            {
                left = ParseTerm();
            }
        }
        else
        {
            left = beginning;
        }

        while (IsBinaryArithmeticOperator(_tok) && GetPrecedence(_tok) > minPrecedence)
        {
            var op = _tok;
            ConsumeToken();
            var right = ParseArithmeticExpr(null, GetPrecedence(op));
            left = new BinaryOpExpr { Operator = op, Left = left, Right = right };
        }
        return left;
    }

    private Term ParseTerm()
    {
        var term = TryParseTerm();
        ExpectTrue(term != null);
        return term!;
    }

    private Term? TryParseTerm()
    {

        if (_tok is IdentifierToken id)
        {
            if (_tokNext is OperatorDot)
            {
                return ParseFunc();
            }
            ConsumeToken();
            return new IdTerm { Id = id.Name };
        }
        else if (_tok is IntegerLiteral intVal)
        {
            ConsumeToken();
            return new IntValueTerm { Value = intVal.Value };
        }
        else if (_tok is BooleanLiteral b)
        {
            ConsumeToken();
            return new BoolValueTerm { Value = b.Value };
        }
        else if (_tok is OperatorLeftParen)
        {
            ConsumeToken();
            var expr = ParseExpr();
            ExpectToken<OperatorRightParen>(_tok);
            ConsumeToken();
            return new ParenTerm { InnerExpr = expr };
        }
        return null;
    }

    private FunctionTerm ParseFunc()
    {
        // expect identifier
        ExpectToken<IdentifierToken>(_tok);
        var param = (IdentifierToken)_tok;
        ConsumeToken();

        // expect dot
        ExpectToken<OperatorDot>(_tok);
        ConsumeToken();
        var body = ParseExpr();
        return new FunctionTerm { Var = param.Name, Def = body };
    }

    private IToken ConsumeToken()
    {
        _tok = _tokNext!;
        _tokNext = _lex.TryGetToken();
        return _tok;
    }

    private static bool IsUnaryArithmeticOperator(IToken token)
    {
        return token is OperatorMinus or OperatorPlus;
    }

    private static bool IsBinaryArithmeticOperator(IToken token)
    {
        return token is
            OperatorPlus or
            OperatorMinus or
            OperatorMultiply or
            OperatorDivide or
            OperatorEq or
            OperatorNe or
            OperatorGt or
            OperatorLt;
    }

    private static int GetPrecedence(IToken token)
    {
        return token switch
        {
            OperatorEq or OperatorNe => 10,
            OperatorAnd or OperatorOr => 20,
            OperatorGt or OperatorLt => 30,
            OperatorPlus or OperatorMinus => 40,
            OperatorMultiply or OperatorDivide => 50,
            _ => throw new ParseError(),
        };
    }

    private static void ExpectToken<T>(IToken token) where T : IToken
    {
        if (token is not T) throw new ParseError();
    }

    private static void ExpectTrue(bool expr)
    {
        if (!expr) throw new ParseError();
    }
}
