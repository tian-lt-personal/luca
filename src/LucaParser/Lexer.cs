using System.Text.RegularExpressions;

namespace Luca.Parser;

public class LexerError : Exception
{
    public int? SourceIndex { get; init; }
    public LexerError(int? index) : base($"general lexer error, index = {index}.") { SourceIndex = index; }
    public LexerError(string message) : base(message) { }
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

public class BadIntegerLiteral : LexerError
{
    public string Literal { get; init; }
    public BadIntegerLiteral(int? index, string literal) : base($"bad integer literal, index = {index}, literal = {literal}")
    {
        SourceIndex = index;
        Literal = literal;
    }
}

internal class Lexer
{
    private const string _pattern =
        @"(?<Keyword>\b(if|then|else|curexpr)\b)|" +
        @"(?<IntegerLiteral>\d+)|" +
        @"(?<Operator>->|[\+\-\*/=<>!])|" +
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
                                    return CreateKeywordToken(group.Value, idx);
                                case "IntegerLiteral":
                                    return CreateIntegerLiteral(group.Value, idx);
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
        throw new LexerError(_idx);
    }

    private static IToken CreateKeywordToken(string name, int index)
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
}
