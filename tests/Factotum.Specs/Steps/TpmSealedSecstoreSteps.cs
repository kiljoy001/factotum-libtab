using Factotum.Testing;
using Reqnroll;
using Xunit;

namespace Factotum.Specs.Steps;

[Binding]
public sealed class TpmSealedSecstoreSteps
{
    private FactotumProcess? factotum;
    private NinePFactotumClient? client;
    private string? keyringSecret;
    private string? sealedBlob;
    private string? unsealReply;

    [Given("a running factotum with the mock TPM backend")]
    public async Task GivenARunningFactotumWithTheMockTpmBackend()
    {
        factotum = await FactotumProcess.StartAsync("mock");
        client = await NinePFactotumClient.ConnectAsync(factotum.SocketPath);

        string protos = await client.ReadTextAsync("proto");
        Assert.Contains("tpmseal", protos.Split('\n', StringSplitOptions.RemoveEmptyEntries));
    }

    [Given("the login keyring secret is {string}")]
    public void GivenTheLoginKeyringSecretIs(string secret)
        => keyringSecret = secret;

    [When("I seal the secstore password {string}")]
    public async Task WhenISealTheSecstorePassword(string password)
    {
        Assert.NotNull(client);
        Assert.NotNull(keyringSecret);

        sealedBlob = await client.SealAsync(password, keyringSecret);
    }

    [Then("the sealed blob should not contain {string}")]
    public void ThenTheSealedBlobShouldNotContain(string secret)
    {
        Assert.NotNull(sealedBlob);
        Assert.DoesNotContain(secret, sealedBlob, StringComparison.Ordinal);
    }

    [Then("unsealing with {string} should fail")]
    public async Task ThenUnsealingWithShouldFail(string auth)
    {
        Assert.NotNull(client);
        Assert.NotNull(sealedBlob);

        unsealReply = await client.UnsealAsync(sealedBlob, auth);
        Assert.StartsWith("error ", unsealReply, StringComparison.Ordinal);
    }

    [Then("unsealing with the login keyring secret should return {string}")]
    public async Task ThenUnsealingWithTheLoginKeyringSecretShouldReturn(string password)
    {
        Assert.NotNull(client);
        Assert.NotNull(keyringSecret);
        Assert.NotNull(sealedBlob);

        unsealReply = await client.UnsealAsync(sealedBlob, keyringSecret);
        Assert.Equal($"ok {password}", unsealReply);
    }

    [AfterScenario]
    public async Task AfterScenario()
    {
        if (client is not null)
        {
            await client.DisposeAsync();
        }

        if (factotum is not null)
        {
            await factotum.DisposeAsync();
        }
    }
}
