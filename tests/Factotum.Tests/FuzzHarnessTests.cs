using Factotum.Testing;
using Xunit;

namespace Factotum.Tests;

public sealed class FuzzHarnessTests
{
    private const int Iterations = 2_000;

    [Theory]
    [InlineData(9)]
    [InlineData(20260904)]
    public void AttributeReplyParserSurvivesRandomBytes(int seed)
    {
        var rng = new Random(seed);
        int cases = 0;

        for (int i = 0; i < Iterations; i++)
        {
            byte[] data = RandomBytes(rng, 256);
            FactotumFuzzTargets.FuzzAttributeReply(data);
            cases++;
        }

        Assert.Equal(Iterations, cases);
    }

    [Theory]
    [InlineData(17)]
    [InlineData(20260904)]
    public void RpcMessageEncoderSurvivesRandomBytes(int seed)
    {
        var rng = new Random(seed);
        int cases = 0;

        for (int i = 0; i < Iterations; i++)
        {
            byte[] data = RandomBytes(rng, 256);
            FactotumFuzzTargets.FuzzRpcMessage(data);
            cases++;
        }

        Assert.Equal(Iterations, cases);
    }

    private static byte[] RandomBytes(Random rng, int maxLength)
    {
        byte[] data = new byte[rng.Next(maxLength + 1)];
        rng.NextBytes(data);
        return data;
    }
}
