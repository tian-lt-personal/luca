using System.Text.RegularExpressions;

namespace Luca.Parser;

public class LexerError : Exception
{
    public int? SourceIndex { get; init; }
    public LexerError(int? index) : base($"general lexer error, index = {index}.") { SourceIndex = index; }
    public LexerError(string message) : base(message) { }
}

public class DrainedError : LexerError
{
    public DrainedError(int? index) : base($"drained at {index}") { }
}

public class BadKeywordError : LexerError
{
    public string Keyword { get; init; }
    public BadKeywordError(int? index, string keyword) : base($"bad keyword token, index = {index}, keyword-name = {keyword}.")
    {
        SourceIndex = index;
        Keyword = keyword;
    }
}

public class BadOperatorError : LexerError
{
    public string Operator { get; init; }
    public BadOperatorError(int? index, string op) : base($"bad operator token, index = {index}, operator-name = {op}.")
    {
        SourceIndex = index;
        Operator = op;
    }
}

public class BadIntegerLiteral : LexerError
{
    public string Literal { get; init; }
    public BadIntegerLiteral(int? index, string literal) : base($"bad integer literal, index = {index}, literal = {literal}")
    {
        SourceIndex = index;
        Literal = literal;
    }
}

internal sealed class Lexer
{
    private const string _pattern =
        @"(?<Keyword>\b(let|if|then|else|curexpr)\b)|" +
        @"(?<Identifier>[A-z_a-z][\w_]*)|" +
        @"(?<IntegerLiteral>\b\d+\b)|" +
        @"(?<Operator>->|!=|[.\+\-\*/=<>%\(\);])|" +
        @"(?<Ws>\s+)";
    private readonly string _source;
    private readonly Regex _dfa;
    private int _idx = 0;

    public Lexer(string source)
    {
        _source = source;
        _dfa = new Regex(_pattern, RegexOptions.Compiled | RegexOptions.ExplicitCapture);
    }

    public IToken GetToken()
    {
        var token = TryGetToken();
        if (token == null)
        {
            throw new DrainedError(_idx);
        }
        return token;
    }

    public IToken? TryGetToken()
    {
        while (_idx < _source.Length)
        {
            var match = _dfa.Match(_source, _idx);
            if (match.Success && match.Index == _idx)
            {
                foreach (var obj in match.Groups)
                {
                    var group = (Group)obj;
                    if (group.Name != "0" && group.Success)
                    {
                        var idx = _idx;
                        _idx += group.Length;
                        if (group.Name != "Ws")
                        {
                            switch (group.Name)
                            {
                                case "Keyword":
                                    return CreateKeyword(group.Value, idx);
                                case "IntegerLiteral":
                                    return CreateIntegerLiteral(group.Value, idx);
                                case "Operator":
                                    return CreateOperator(group.Value, idx);
                                case "Identifier":
                                    return new IdentifierToken(group.Value);
                                default:
                                    throw new ArgumentException();
                            }
                        }
                    }
                }
            }
            else
            {
                throw new LexerError(_idx);
            }
        }
        return null;
    }

    private static IToken CreateKeyword(string name, int index)
    {
        switch (name)
        {
            case "if":
                return new KeywordIf();
            case "then":
                return new KeywordThen();
            case "else":
                return new KeywordElse();
            case "curexpr":
                return new KeywordCurexpr();
            case "let":
                return new KeywordLet();
            default:
                throw new BadKeywordError(index, name);
        }
    }

    private static IToken CreateIntegerLiteral(string literal, int index)
    {
        try
        {
            return new IntegerLiteral(literal);
        }
        catch (FormatException)
        {
            throw new BadIntegerLiteral(index, literal);
        }
    }

    private static IToken CreateOperator(string name, int index)
    {
        switch (name)
        {
            case "->":
                return new OperatorPropOf();
            case ".":
                return new OperatorDot();
            case "+":
                return new OperatorPlus();
            case "-":
                return new OperatorMinus();
            case "*":
                return new OperatorMultiply();
            case "/":
                return new OperatorDivide();
            case "%":
                return new OperatorRem();
            case "=":
                return new OperatorEq();
            case "!=":
                return new OperatorNe();
            case "(":
                return new OperatorLeftParen();
            case ")":
                return new OperatorRightParen();
            case ";":
                return new OperatorSemicolon();
            default:
                throw new BadOperatorError(index, name);
        }
    }
}
