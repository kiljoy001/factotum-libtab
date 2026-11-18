using Factotum.Testing;
using FsCheck;
using FsCheck.Xunit;
using Xunit;

namespace Factotum.Tests;

public sealed class FactotumAttributeCodecPropertyTests
{
    [Fact]
    public void TokenRejectsNull()
    {
        Assert.Throws<ArgumentNullException>(() =>
        {
            _ = FactotumAttributeCodec.Token(null!);
        });
    }

    [Theory]
    [InlineData("")]
    [InlineData("bad value")]
    [InlineData("bad\tvalue")]
    [InlineData("bad'value")]
    [InlineData("bad\"value")]
    public void TokenRejectsUnsafeValues(string value)
    {
        ArgumentException error = Assert.Throws<ArgumentException>(() =>
        {
            _ = FactotumAttributeCodec.Token(value);
        });

        Assert.StartsWith(
            "factotum attributes in this harness require non-empty unquoted tokens",
            error.Message,
            StringComparison.Ordinal);
    }

    [Property(MaxTest = 500)]
    public Property SafeTokensRoundTripAsAttributeValues()
        => Prop.ForAll(Generators.SafeToken(), token =>
        {
            string reply = $"ok name=secstore blob={token} policy=none";
            return FactotumAttributeCodec.Attribute(reply, "blob") == token;
        });

    [Property(MaxTest = 500)]
    public Property IllegalTokensAreRejected()
        => Prop.ForAll(Generators.IllegalToken(), token =>
        {
            Exception? error = Record.Exception(() => FactotumAttributeCodec.Token(token));
            return error is ArgumentException;
        });

    [Fact]
    public void QuotedAttributesCanContainSpaces()
    {
        string value = FactotumAttributeCodec.Attribute(
            "ok name='secstore password' blob=mock-v1:YWJj",
            "name");

        Assert.Equal("secstore password", value);
    }

    [Fact]
    public void TokensIgnoreWhitespaceOnlyInput()
    {
        IReadOnlyList<string> tokens = FactotumAttributeCodec.Tokens(" \t\r\n");

        Assert.Empty(tokens);
    }

    [Fact]
    public void TokensParseMultipleAttributes()
    {
        IReadOnlyList<string> tokens = FactotumAttributeCodec.Tokens(
            " ok name='secstore password' blob=mock-v1:YWJj ");

        Assert.Equal(
            new[] { "ok", "name=secstore password", "blob=mock-v1:YWJj" },
            tokens);
    }

    [Fact]
    public void TokensRejectNullLine()
    {
        Assert.Throws<ArgumentNullException>(() =>
        {
            _ = FactotumAttributeCodec.Tokens(null!);
        });
    }

    [Fact]
    public void TryGetAttributeRejectsNullInputs()
    {
        Assert.Throws<ArgumentNullException>(() =>
        {
            _ = FactotumAttributeCodec.TryGetAttribute(null!, "blob", out _);
        });

        Assert.Throws<ArgumentNullException>(() =>
        {
            _ = FactotumAttributeCodec.TryGetAttribute("ok blob=value", null!, out _);
        });
    }

    [Fact]
    public void MissingAttributeFailsClearly()
    {
        Exception error = Assert.Throws<InvalidOperationException>(() =>
            FactotumAttributeCodec.Attribute("ok name=secstore", "blob"));

        Assert.Contains("missing blob", error.Message, StringComparison.Ordinal);
    }
}
