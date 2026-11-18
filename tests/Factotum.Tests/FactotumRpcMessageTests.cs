using System.Text;
using Factotum.Testing;
using FsCheck;
using FsCheck.Xunit;
using Xunit;

namespace Factotum.Tests;

public sealed class FactotumRpcMessageTests
{
    [Fact]
    public void EncodeRejectsNullVerb()
    {
        Assert.Throws<ArgumentNullException>(() =>
        {
            _ = FactotumRpcMessage.Encode(null!);
        });
    }

    [Theory]
    [InlineData("")]
    [InlineData("bad verb")]
    [InlineData("bad\tverb")]
    public void EncodeRejectsInvalidVerb(string verb)
    {
        ArgumentException error = Assert.Throws<ArgumentException>(() =>
        {
            _ = FactotumRpcMessage.Encode(verb);
        });

        Assert.StartsWith(
            "RPC verb must be a non-empty token",
            error.Message,
            StringComparison.Ordinal);
    }

    [Fact]
    public void EncodeWithoutPayloadWritesOnlyTheVerb()
    {
        byte[] encoded = FactotumRpcMessage.Encode("read");

        Assert.Equal("read", Encoding.UTF8.GetString(encoded));
    }

    [Property(MaxTest = 500)]
    public Property EncodeWithPayloadWritesOneSeparator()
        => Prop.ForAll(Generators.RpcVerb(), Generators.Payload(), (verb, payload) =>
        {
            byte[] encoded = FactotumRpcMessage.Encode(verb, payload);
            string text = Encoding.UTF8.GetString(encoded);

            return text == $"{verb} {payload}";
        });

    [Fact]
    public void RequireOkRejectsErrors()
    {
        Exception error = Assert.Throws<InvalidOperationException>(() =>
            FactotumRpcMessage.RequireOk("error bad TPM auth", "unseal"));

        Assert.Contains("unseal", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void RequireOkPrefixAcceptsOkPayload()
    {
        Exception? error = Record.Exception(() =>
            FactotumRpcMessage.RequireOkPrefix("ok mock-v2:blob", "read blob"));

        Assert.Null(error);
    }

    [Fact]
    public void RequireOkPrefixRejectsPlainOk()
    {
        Exception error = Assert.Throws<InvalidOperationException>(() =>
            FactotumRpcMessage.RequireOkPrefix("ok", "read blob"));

        Assert.Contains("read blob", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void RequireOkPrefixRejectsErrors()
    {
        Exception error = Assert.Throws<InvalidOperationException>(() =>
            FactotumRpcMessage.RequireOkPrefix("error bad blob", "read blob"));

        Assert.Contains("error bad blob", error.Message, StringComparison.Ordinal);
    }
}
