using System.Text;

namespace Factotum.Testing;

public static class FactotumRpcMessage
{
    public static byte[] Encode(string verb, string? payload = null)
    {
        ArgumentNullException.ThrowIfNull(verb);

        if (!IsVerb(verb))
        {
            throw new ArgumentException("RPC verb must be a non-empty token", nameof(verb));
        }

        if (payload is null)
        {
            return Encoding.UTF8.GetBytes(verb);
        }

        byte[] verbBytes = Encoding.UTF8.GetBytes(verb);
        byte[] payloadBytes = Encoding.UTF8.GetBytes(payload);
        byte[] data = new byte[verbBytes.Length + 1 + payloadBytes.Length];

        Buffer.BlockCopy(verbBytes, 0, data, 0, verbBytes.Length);
        data[verbBytes.Length] = (byte)' ';
        Buffer.BlockCopy(payloadBytes, 0, data, verbBytes.Length + 1, payloadBytes.Length);
        return data;
    }

    public static void RequireOk(string reply, string operation)
    {
        if (reply != "ok")
        {
            throw new InvalidOperationException($"{operation}: got {reply}");
        }
    }

    public static void RequireOkPrefix(string reply, string operation)
    {
        if (!reply.StartsWith("ok ", StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"{operation}: got {reply}");
        }
    }

    private static bool IsVerb(string verb)
        => verb.Length > 0 && verb.All(c => !char.IsWhiteSpace(c));
}
