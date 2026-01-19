using Luca.Parser;

namespace Luca.Cli;

public static class Program
{
    public static void Main(string[] args)
    {
        var vm = new Machine();
        Console.Write("> ");
        while (true)
        {
            var line = Console.ReadLine();
            if (line is null)
            {
                break;
            }
            if (line.TrimEnd().EndsWith(";"))
            {
                try
                {
                    vm.Digest(line);
                }
                catch (Exception ex) when (ex is MachineRuntimeError or ParseError or LexerError)
                {
                    Console.WriteLine($"VM Error: {ex.Message}");
                }
                Console.Write("> ");
            }
        }
    }
}
