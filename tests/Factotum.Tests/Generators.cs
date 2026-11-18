using FsCheck;

namespace Factotum.Tests;

internal static class Generators
{
    private static readonly char[] TokenChars =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:-+/=".ToCharArray();

    public static Arbitrary<string> SafeToken()
        => Arb.From(SafeTokenGen());

    public static Arbitrary<string> IllegalToken()
        => Arb.From(Gen.OneOf(
            Gen.Constant(string.Empty),
            from prefix in SafeTokenGen()
            from bad in Gen.Elements(' ', '\t', '\n', '\r', '\'', '"')
            from suffix in SafeTokenGen()
            select prefix + bad + suffix));

    public static Arbitrary<string> RpcVerb()
        => Arb.From(Gen.Elements("start", "read", "write", "readhex", "writehex"));

    public static Arbitrary<string> Payload()
        => Arb.From(
            from length in Gen.Choose(0, 96)
            from chars in Gen.ArrayOf(length, Gen.Elements(TokenChars))
            select new string(chars));

    private static Gen<string> SafeTokenGen()
        => from length in Gen.Choose(1, 96)
           from chars in Gen.ArrayOf(length, Gen.Elements(TokenChars))
           select new string(chars);
}
