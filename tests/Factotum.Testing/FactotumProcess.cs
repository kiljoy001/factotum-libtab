using System.Diagnostics;
using System.Text;

namespace Factotum.Testing;

public sealed class FactotumProcess : IAsyncDisposable
{
    private readonly StringBuilder log = new();
    private readonly string namespaceDir;
    private readonly Process process;
    private readonly string serviceName;

    private FactotumProcess(string namespaceDir, string serviceName, string socketPath, Process process)
    {
        this.namespaceDir = namespaceDir;
        this.serviceName = serviceName;
        SocketPath = socketPath;
        this.process = process;
    }

    public string SocketPath { get; }

    public static async Task<FactotumProcess> StartAsync(string backend, CancellationToken cancellationToken = default)
    {
        string repoRoot = FindRepoRoot();
        await EnsureBuiltAsync(repoRoot, cancellationToken);

        string namespaceDir = Path.Combine(Path.GetTempPath(), $"factotum-spec-{Guid.NewGuid():N}");
        Directory.CreateDirectory(namespaceDir);
        TrySetOwnerOnly(namespaceDir);

        string serviceName = $"factotum-spec-{Environment.ProcessId}-{Guid.NewGuid():N}";
        string socketPath = Path.Combine(namespaceDir, serviceName);
        var start = new ProcessStartInfo(Path.Combine(repoRoot, "factotum"))
        {
            WorkingDirectory = repoRoot,
            RedirectStandardError = true,
            RedirectStandardOutput = true,
            UseShellExecute = false,
        };

        start.ArgumentList.Add("-n");
        start.ArgumentList.Add("-s");
        start.ArgumentList.Add(serviceName);
        start.Environment["FACTOTUM_TPM_BACKEND"] = backend;
        start.Environment["NAMESPACE"] = namespaceDir;
        start.Environment["PLAN9"] = "/usr/local/plan9";
        start.Environment["PATH"] = $"{repoRoot}:/usr/local/plan9/bin:{Environment.GetEnvironmentVariable("PATH")}";

        var process = new Process { StartInfo = start, EnableRaisingEvents = true };
        var factotum = new FactotumProcess(namespaceDir, serviceName, socketPath, process);

        process.OutputDataReceived += (_, e) => factotum.AppendLog(e.Data);
        process.ErrorDataReceived += (_, e) => factotum.AppendLog(e.Data);

        if (!process.Start())
        {
            throw new InvalidOperationException("could not start factotum");
        }

        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        try
        {
            await factotum.WaitForSocketAsync(cancellationToken);
            return factotum;
        }
        catch
        {
            await factotum.DisposeAsync();
            throw;
        }
    }

    public async ValueTask DisposeAsync()
    {
        KillProcess(process);
        KillRelatedProcesses(serviceName);

        try
        {
            await process.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(2));
        }
        catch
        {
            KillProcess(process);
        }

        process.Dispose();
        TryDeleteNamespace();
    }

    private static string FindRepoRoot()
    {
        string? configured = Environment.GetEnvironmentVariable("FACTOTUM_REPO_ROOT");
        if (!string.IsNullOrWhiteSpace(configured))
        {
            return Path.GetFullPath(configured);
        }

        DirectoryInfo? dir = new(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "Makefile")) &&
                Directory.Exists(Path.Combine(dir.FullName, "src", "cmd", "auth", "factotum")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        throw new InvalidOperationException("could not locate the factotum-dp9ik repository root");
    }

    private static async Task EnsureBuiltAsync(string repoRoot, CancellationToken cancellationToken)
    {
        if (Environment.GetEnvironmentVariable("FACTOTUM_SPEC_SKIP_MAKE") == "1")
        {
            return;
        }

        var start = new ProcessStartInfo("/usr/bin/make")
        {
            WorkingDirectory = repoRoot,
            RedirectStandardError = true,
            RedirectStandardOutput = true,
            UseShellExecute = false,
        };
        start.ArgumentList.Add("factotum");

        using var process = Process.Start(start) ?? throw new InvalidOperationException("could not start make");
        string stdout = await process.StandardOutput.ReadToEndAsync(cancellationToken);
        string stderr = await process.StandardError.ReadToEndAsync(cancellationToken);
        await process.WaitForExitAsync(cancellationToken);

        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException($"make factotum failed with exit code {process.ExitCode}\n{stdout}{stderr}");
        }
    }

    private async Task WaitForSocketAsync(CancellationToken cancellationToken)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromSeconds(5));

        while (!timeout.IsCancellationRequested)
        {
            if (File.Exists(SocketPath))
            {
                return;
            }

            if (process.HasExited)
            {
                throw new InvalidOperationException($"factotum exited before posting {SocketPath}\n{log}");
            }

            try
            {
                await Task.Delay(TimeSpan.FromMilliseconds(50), timeout.Token);
            }
        catch (OperationCanceledException)
        {
            // The loop below turns the timeout into a message that includes factotum logs.
        }
        }

        throw new TimeoutException($"factotum did not post {SocketPath}\n{log}");
    }

    private static void KillRelatedProcesses(string serviceName)
    {
        if (!Directory.Exists("/proc"))
        {
            return;
        }

        foreach (Process candidate in Process.GetProcesses())
        {
            if (candidate.Id == Environment.ProcessId)
            {
                continue;
            }

            try
            {
                string cmdline = File.ReadAllText($"/proc/{candidate.Id}/cmdline").Replace('\0', ' ');
                if (cmdline.Contains(serviceName, StringComparison.Ordinal) &&
                    (cmdline.Contains("factotum", StringComparison.Ordinal) || cmdline.Contains("9pserve", StringComparison.Ordinal)))
                {
                    KillProcess(candidate);
                }
            }
            catch
            {
                // Processes can exit between enumeration and /proc inspection.
                candidate.Dispose();
            }
        }
    }

    private static void KillProcess(Process candidate)
    {
        try
        {
            if (!candidate.HasExited)
            {
                candidate.Kill(entireProcessTree: true);
            }
        }
        catch
        {
            // Cleanup is best effort; the process may already have exited.
        }
    }

    private static void TrySetOwnerOnly(string path)
    {
        if (OperatingSystem.IsWindows())
        {
            return;
        }

        try
        {
            File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute);
        }
        catch
        {
            // Some filesystems do not support Unix permission updates.
        }
    }

    private void AppendLog(string? line)
    {
        if (line is not null)
        {
            log.AppendLine(line);
        }
    }

    private void TryDeleteNamespace()
    {
        try
        {
            File.Delete(SocketPath);
            Directory.Delete(namespaceDir);
        }
        catch
        {
            // The service socket may already have been removed by factotum or 9pserve.
        }
    }
}
