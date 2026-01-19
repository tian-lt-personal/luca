using Luca.Parser;
using Luca.Parser.Ast;

namespace Luca.Cli;

public static class Program
{
    public static void Main(string[] args)
    {
        var vm = new Machine();
        Console.Write("> ");
        string source = string.Empty;
        while (true)
        {
            var line = Console.ReadLine();
            if (line is null)
            {
                break;
            }
            source += line.TrimEnd();
            if (source.EndsWith(";"))
            {
                bool print = source.EndsWith(";;");
                if (print) { source = source[..^1]; }
                try
                {
                    foreach (var result in vm.Digest(source))
                    {
                        if (print)
                        {
                            Console.WriteLine(AstUtils.Dump(result));
                        }
                    }
                }
                catch (Exception ex) when (ex is MachineRuntimeError or ParseError or LexerError)
                {
                    Console.WriteLine($"VM Error: {ex.Message}");
                }
                source = string.Empty;
                Console.Write("> ");
            }
        }
    }
}
