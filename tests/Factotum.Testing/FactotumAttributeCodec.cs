using System.Text;

namespace Factotum.Testing;

public static class FactotumAttributeCodec
{
    public static string Token(string value)
    {
        ArgumentNullException.ThrowIfNull(value);

        if (!IsToken(value))
        {
            throw new ArgumentException("factotum attributes in this harness require non-empty unquoted tokens", nameof(value));
        }

        return value;
    }

    public static bool IsToken(string value)
        => value.Length > 0 && value.All(IsTokenChar);

    public static string Attribute(string reply, string name)
    {
        if (TryGetAttribute(reply, name, out string? value) && value is not null)
        {
            return value;
        }

        throw new InvalidOperationException($"missing {name} in RPC reply: {reply}");
    }

    public static bool TryGetAttribute(string reply, string name, out string? value)
    {
        ArgumentNullException.ThrowIfNull(reply);
        ArgumentNullException.ThrowIfNull(name);

        foreach (string token in Tokens(reply))
        {
            int equals = token.IndexOf('=', StringComparison.Ordinal);
            if (equals == name.Length && token.StartsWith(name, StringComparison.Ordinal))
            {
                value = token[(equals + 1)..];
                return true;
            }
        }

        value = null;
        return false;
    }

    public static IReadOnlyList<string> Tokens(string line)
    {
        ArgumentNullException.ThrowIfNull(line);

        var tokens = new List<string>();
        int index = 0;
        while (index < line.Length)
        {
            while (index < line.Length && char.IsWhiteSpace(line[index]))
            {
                index++;
            }

            if (index >= line.Length)
            {
                break;
            }

            tokens.Add(ReadToken(line, ref index));
        }

        return tokens;
    }

    private static string ReadToken(string line, ref int index)
    {
        var token = new StringBuilder();

        while (index < line.Length && !char.IsWhiteSpace(line[index]))
        {
            char c = line[index++];
            if (c == '\'' || c == '"')
            {
                ReadQuoted(line, ref index, c, token);
            }
            else
            {
                token.Append(c);
            }
        }

        return token.ToString();
    }

    private static void ReadQuoted(string line, ref int index, char quote, StringBuilder token)
    {
        while (index < line.Length)
        {
            char c = line[index++];
            if (c == quote)
            {
                return;
            }

            token.Append(c);
        }
    }

    private static bool IsTokenChar(char c)
        => !char.IsWhiteSpace(c) && c != '\'' && c != '"';
}
