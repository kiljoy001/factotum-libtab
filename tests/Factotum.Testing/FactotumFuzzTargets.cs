using System.Text;

namespace Factotum.Testing;

public static class FactotumFuzzTargets
{
    public static void FuzzAttributeReply(ReadOnlySpan<byte> data)
    {
        string text = Encoding.UTF8.GetString(data);
        IReadOnlyList<string> tokens = FactotumAttributeCodec.Tokens(text);

        if (FactotumAttributeCodec.TryGetAttribute(text, "blob", out string? blob) &&
            (tokens.Count == 0 || blob is null))
        {
            throw new InvalidOperationException("attribute parser returned a value from no tokens");
        }
    }

    public static void FuzzRpcMessage(ReadOnlySpan<byte> data)
    {
        if (data.Length == 0)
        {
            return;
        }

        string verb = DecodeVerb(data[0]);
        string payload = Encoding.UTF8.GetString(data[1..]);
        byte[] encoded = FactotumRpcMessage.Encode(verb, payload);

        if (encoded.Length != verb.Length + 1 + Encoding.UTF8.GetByteCount(payload))
        {
            throw new InvalidOperationException("RPC message length is inconsistent with its inputs");
        }
    }

    private static string DecodeVerb(byte value)
        => (value % 5) switch
        {
            0 => "start",
            1 => "read",
            2 => "write",
            3 => "readhex",
            _ => "writehex",
        };
}
