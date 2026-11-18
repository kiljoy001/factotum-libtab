using System.Net.Sockets;
using System.Text;
using NinePSharp.Client;
using NinePSharp.Constants;

namespace Factotum.Testing;

public sealed class NinePFactotumClient : IAsyncDisposable
{
    private readonly NinePClient client;
    private readonly uint rootFid;

    private NinePFactotumClient(NinePClient client, uint rootFid)
    {
        this.client = client;
        this.rootFid = rootFid;
    }

    public static async Task<NinePFactotumClient> ConnectAsync(string socketPath, CancellationToken cancellationToken = default)
    {
        var socket = new Socket(AddressFamily.Unix, SocketType.Stream, ProtocolType.Unspecified);
        await socket.ConnectAsync(new UnixDomainSocketEndPoint(socketPath), cancellationToken);
        var stream = new NetworkStream(socket, ownsSocket: true);
        var client = new NinePClient(stream);

        await client.VersionAsync(NinePConstants.DefaultMSize, NinePConstants.VersionString_9p);
        uint rootFid = client.GetNextFid();
        await client.AttachAsync(rootFid, NinePConstants.NoFid, Environment.UserName, "/");
        return new NinePFactotumClient(client, rootFid);
    }

    public async ValueTask DisposeAsync()
    {
        try
        {
            await client.ClunkAsync(rootFid);
        }
        catch
        {
            // The connection may already be closed when scenario cleanup runs.
        }

        client.Dispose();
    }

    public async Task<string> ReadTextAsync(string path)
    {
        uint fid = await OpenPathAsync(path, NinePConstants.OREAD);
        try
        {
            var bytes = new List<byte>();
            ulong offset = 0;
            uint chunkSize = client.MSize - 24;

            while (true)
            {
                var read = await client.ReadAsync(fid, offset, chunkSize);
                if (read.Count == 0)
                {
                    break;
                }

                bytes.AddRange(read.Data.ToArray());
                offset += read.Count;
            }

            return Encoding.UTF8.GetString(bytes.ToArray());
        }
        finally
        {
            await ClunkQuietlyAsync(fid);
        }
    }

    public async Task<string> SealAsync(string password, string auth)
    {
        uint fid = await OpenPathAsync("rpc", NinePConstants.ORDWR);
        try
        {
            string start = await RpcRoundTripAsync(fid, "start", $"proto=tpmseal role=seal name=secstore policy=none !auth={FactotumAttributeCodec.Token(auth)}");
            FactotumRpcMessage.RequireOk(start, "start tpmseal seal");

            string write = await RpcRoundTripAsync(fid, "write", password);
            FactotumRpcMessage.RequireOk(write, "write tpmseal secret");

            string read = await RpcRoundTripAsync(fid, "read");
            FactotumRpcMessage.RequireOkPrefix(read, "read tpmseal blob");
            return FactotumAttributeCodec.Attribute(read, "blob");
        }
        finally
        {
            await ClunkQuietlyAsync(fid);
        }
    }

    public async Task<string> UnsealAsync(string blob, string auth)
    {
        uint fid = await OpenPathAsync("rpc", NinePConstants.ORDWR);
        try
        {
            string start = await RpcRoundTripAsync(fid, "start", $"proto=tpmseal role=unseal name=secstore blob={FactotumAttributeCodec.Token(blob)} !auth={FactotumAttributeCodec.Token(auth)}");
            FactotumRpcMessage.RequireOk(start, "start tpmseal unseal");
            return await RpcRoundTripAsync(fid, "read");
        }
        finally
        {
            await ClunkQuietlyAsync(fid);
        }
    }

    private async Task<uint> OpenPathAsync(string path, byte mode)
    {
        uint fid = await WalkPathAsync(path);

        try
        {
            await client.OpenAsync(fid, mode);
            return fid;
        }
        catch
        {
            await ClunkQuietlyAsync(fid);
            throw;
        }
    }

    private async Task<uint> WalkPathAsync(string path)
    {
        string[] names = path.Split('/', StringSplitOptions.RemoveEmptyEntries);
        uint fid = client.GetNextFid();
        var walk = await client.WalkAsync(rootFid, fid, names);
        if (walk.Wqid.Length != names.Length)
        {
            await ClunkQuietlyAsync(fid);
            throw new InvalidOperationException($"could not walk {path}");
        }

        return fid;
    }

    private async Task<string> RpcRoundTripAsync(uint fid, string verb, string? payload = null)
    {
        byte[] data = FactotumRpcMessage.Encode(verb, payload);
        var write = await client.WriteAsync(fid, 0, data);
        if (write.Count != data.Length)
        {
            throw new InvalidOperationException($"short RPC write for {verb}: {write.Count} of {data.Length}");
        }

        var read = await client.ReadAsync(fid, 0, client.MSize - 24);
        return Encoding.UTF8.GetString(read.Data.Span);
    }

    private async Task ClunkQuietlyAsync(uint fid)
    {
        try
        {
            await client.ClunkAsync(fid);
        }
        catch
        {
            // Failed operations can leave fids already clunked server-side.
        }
    }
}
