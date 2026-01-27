using System.CommandLine;
using System.Reflection;
using Luca.Parser;

namespace Luca.Cli;

public static class Program
{
    public static void Main(string[] args)
    {
        var rootCmd = new RootCommand("LUCA CLI");
        rootCmd.SetAction(parseResult => RealtimeCli());
        rootCmd.Parse(args).Invoke();
    }

    private static void RealtimeCli()
    {
        var vm = new Machine();
        var ver = Assembly.GetEntryAssembly()!.GetCustomAttribute<AssemblyInformationalVersionAttribute>()!.InformationalVersion;
        Console.WriteLine($"LUCA CLI ({ver})\nEnter commands ending with ';;' to execute.\n");
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
                            Console.WriteLine(Machine.Dump(result));
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
