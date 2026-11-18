using Factotum.Testing;
using SharpFuzz;

namespace Factotum.Fuzz;

public static class Program
{
    public static void Main(string[] args)
    {
        string target = args.Length > 0 ? args[0] : "attr";
        Action<Stream> run = target switch
        {
            "rpc" => FuzzRpcMessage,
            _ => FuzzAttributeReply,
        };

        Fuzzer.Run(run);
    }

    private static void FuzzAttributeReply(Stream stream)
        => FactotumFuzzTargets.FuzzAttributeReply(ReadAll(stream));

    private static void FuzzRpcMessage(Stream stream)
        => FactotumFuzzTargets.FuzzRpcMessage(ReadAll(stream));

    private static byte[] ReadAll(Stream stream)
    {
        using var buffer = new MemoryStream();
        stream.CopyTo(buffer);
        return buffer.ToArray();
    }
}
