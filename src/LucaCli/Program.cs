using System.CommandLine;
using System.Reflection;
using Luca.Parser;

namespace Luca.Cli;

public static class Program
{
    public static void Main(string[] args)
    {
        var fileArg = new Argument<FileInfo>(name: "file")
        {
            Arity = ArgumentArity.ZeroOrOne,
            Description = "LUCA source file"
        };
        fileArg.AcceptExistingOnly();
        var rootCmd = new RootCommand("LUCA CLI");
        rootCmd.Arguments.Add(fileArg);
        rootCmd.SetAction(parseResult =>
        {
            var file = parseResult.GetValue(fileArg);
            try
            {
                if (file != null)
                {
                    ExecuteFile(file);
                }
                else
                {
                    RealtimeCli();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Runtime Error: {ex.Message}");
            }
        });
        rootCmd.Parse(args).Invoke();
    }

    private static void ExecuteFile(FileInfo file)
    {
        var vm = new Machine();
        var source = File.ReadAllText(file.FullName);
        try
        {
            foreach (var _ in vm.Digest(source)) { }
        }
        catch (Exception ex) when (ex is MachineRuntimeError or ParseError or LexerError)
        {
            Console.WriteLine($"VM Error: {ex.Message}");
        }
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
