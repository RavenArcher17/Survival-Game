// Wildreach-Setup.exe when the whole Setup is over GitHub's 2 GiB limit for one release file: the release then has the
// Setup cut into parts (Wildreach-Setup.001, .002... and the list Wildreach-Setup.parts, see SetupParts in
// WildreachPack.cs), and this small program is what a player downloads. It fetches the parts over a few connections,
// puts the Setup back together in the temp folder, checks its SHA-256 against the list, and runs it with the same
// arguments, returning its exit code. So:
//   - a new player double-clicks it: a window shows the download, then the Setup's own window takes over;
//   - an installed copy's updater from before the parts (0.7.1 and older) finds no index on this file, downloads it
//     whole and runs it with --silent --update --nolaunch --dir <its folder>: this shows its download window, then the
//     Setup installs silently over that folder as it always did;
//   - updaters from 0.8.0 on read the parts themselves (SetupSource) and never run this.
// The release it belongs to is built in (BootInfo.Tag, written by Tools\make_installer.ps1).
//   --feed <url>   a stand-in for https://github.com/RavenArcher17/Survival-Game (the updater test); not passed on.
//                  WILDREACH_FEED in the environment does the same (an older updater running it passes no --feed).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Threading;
using System.Windows.Forms;
using WildreachPack;

static class WildreachBootstrap
{
	const string Repo = "https://github.com/RavenArcher17/Survival-Game";
	const int Connections = 4;                  // as the updater: GitHub gives each connection about the same speed
	const long Piece = 8L << 20;                // no request longer than this, so the connections share the work
	static string feed = Repo, logPath;
	static bool silent;
	static readonly List<string> passOn = new List<string>();

	[STAThread]
	static int Main(string[] args)
	{
		// The updater test's stand-in feed reaches it this way when an older updater runs it (they don't pass --feed on).
		string envFeed = Environment.GetEnvironmentVariable("WILDREACH_FEED");
		if (!string.IsNullOrEmpty(envFeed)) feed = envFeed.TrimEnd('/');
		for (int i = 0; i < args.Length; i++)
		{
			if (args[i] == "--feed" && i + 1 < args.Length) feed = args[++i].TrimEnd('/');
			else passOn.Add(args[i]);
		}
		silent = passOn.Contains("--silent");
		string temp = Path.Combine(Path.GetTempPath(), "Wildreach-Update");
		Directory.CreateDirectory(temp);
		logPath = Path.Combine(temp, "bootstrap.log");
		ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
		ServicePointManager.DefaultConnectionLimit = Math.Max(ServicePointManager.DefaultConnectionLimit, Connections * 2);
		Log("Wildreach " + BootInfo.Tag + " download program, args: " + string.Join(" ", passOn));
		Application.EnableVisualStyles();
		var form = new BootForm(temp);
		Application.Run(form);
		return form.ExitCode;
	}

	static void Log(string line)
	{
		try { File.AppendAllText(logPath, DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss ") + line + "\r\n"); } catch { }
	}

	static string Quote(string a) { return a.Contains(" ") ? "\"" + a + "\"" : a; }

	class BootForm : Form
	{
		readonly string temp;
		readonly Label label = new Label { Left = 16, Top = 14, Width = 470, Height = 44 };
		readonly ProgressBar bar = new ProgressBar { Left = 16, Top = 62, Width = 470, Height = 22, Maximum = 1000 };
		readonly Button cancel = new Button { Left = 386, Top = 96, Width = 100, Height = 28, Text = "Cancel" };
		volatile bool cancelled;
		long received;
		public int ExitCode = 1;
		string Version { get { return BootInfo.Tag.TrimStart('v', 'V'); } }

		public BootForm(string temp)
		{
			this.temp = temp;
			Text = "Wildreach " + Version;
			Width = 520; Height = 178;
			FormBorderStyle = FormBorderStyle.FixedDialog;
			MaximizeBox = false; MinimizeBox = true;
			StartPosition = FormStartPosition.CenterScreen;
			try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
			label.Text = "Getting Wildreach " + Version + " ready to install...";
			Controls.Add(label); Controls.Add(bar); Controls.Add(cancel);
			cancel.Click += (s, e) => { cancelled = true; Log("cancelled by the player"); ExitCode = 2; Close(); };
			// Closed by the player before it finished: cancelled (the updater that ran it plays on as it was).
			FormClosing += (s, e) => { if (!finished) { cancelled = true; ExitCode = 2; } };
			Shown += (s, e) => new Thread(Work) { IsBackground = true }.Start();
		}

		bool done, finished;

		void Ui(Action a) { try { if (!IsDisposed) BeginInvoke(a); } catch { } }

		void Work()
		{
			string whole = Path.Combine(temp, "Wildreach-Setup-" + BootInfo.Tag + "-whole.exe");
			try
			{
				SetupSource source = null;
				for (int attempt = 1; source == null; attempt++)
				{
					try { source = SetupSource.Find(feed + "/releases/download/" + Uri.EscapeDataString(BootInfo.Tag), "WildreachBootstrap"); }
					catch (WebException e)
					{
						var r = e.Response as HttpWebResponse;
						if (attempt >= 4 || (r != null && r.StatusCode == HttpStatusCode.NotFound)) throw;
						Log("couldn't reach the release (" + e.Message + "), try " + attempt);
						Thread.Sleep(1500 * attempt);
					}
					catch (IOException e) { if (attempt >= 4) throw; Log("couldn't read the release (" + e.Message + "), try " + attempt); Thread.Sleep(1500 * attempt); }
				}
				if (source.Parts == null) throw new InvalidDataException("this release has no Setup parts to download");
				SetupParts list = source.Parts;
				Log(string.Format("{0} parts, {1:N0} MB", list.Parts.Count, list.Size >> 20));
				long free = new DriveInfo(Path.GetPathRoot(temp)).AvailableFreeSpace;
				if (free < list.Size + (256L << 20)) throw new IOException(string.Format("The download needs {0:N0} MB of free space on {1} and there's {2:N0} MB.", (list.Size + (256L << 20)) >> 20, Path.GetPathRoot(temp), free >> 20));

				// An earlier try's download is kept if it's already right (a cancelled install, say).
				if (!(File.Exists(whole) && new FileInfo(whole).Length == list.Size && Sha(whole, false) == list.Sha))
				{
					string part = whole + ".part";
					using (var file = new FileStream(part, FileMode.Create, FileAccess.Write, FileShare.ReadWrite)) file.SetLength(list.Size);
					var pieces = new List<long>();
					for (long at = 0; at < list.Size; at += Piece) pieces.Add(at);
					AllAtOnce(pieces, at => Fetch(source, part, at, Math.Min(Piece, list.Size - at)), list.Size);
					if (cancelled) return;
					Ui(() => { label.Text = "Checking the download..."; bar.Value = 0; });
					string sha = Sha(part, true);
					if (sha != list.Sha) { try { File.Delete(part); } catch { } throw new InvalidDataException("the download came out wrong (its SHA-256 doesn't match the release's list); try again"); }
					if (File.Exists(whole)) File.Delete(whole);
					File.Move(part, whole);
				}
				if (cancelled) return;
				Log("downloaded and checked; starting the Setup");
				done = true;
				Ui(() => { label.Text = "Installing Wildreach " + Version + "..."; bar.Style = ProgressBarStyle.Marquee; cancel.Enabled = false; });
				int code;
				using (Process p = Process.Start(new ProcessStartInfo(whole, string.Join(" ", passOn.Select(Quote))) { UseShellExecute = false }))
				{
					// The Setup's own window comes up for a new player; ours steps aside for it.
					if (!silent) Ui(() => Hide());
					p.WaitForExit();
					code = p.ExitCode;
				}
				Log("the Setup finished with code " + code);
				try { File.Delete(whole); } catch { }
				ExitCode = code;
				finished = true;
				Ui(Close);
			}
			catch (Exception e)
			{
				Log("failed: " + e.Message);
				if (cancelled) return;
				ExitCode = 1;
				finished = true;
				Ui(() =>
				{
					if (!silent) MessageBox.Show(this, "Couldn't download Wildreach " + Version + ":\n" + e.Message + "\n\nCheck the connection and run this again; what came down right is kept only when it's complete.", "Wildreach");
					Close();
				});
			}
		}

		string Sha(string path, bool show)
		{
			using (SHA256 sha = new SHA256Cng())
			using (var input = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20))
			{
				var buffer = new byte[4 << 20];
				long total = input.Length, at = 0;
				int read;
				while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
				{
					if (cancelled) throw new OperationCanceledException();
					sha.TransformBlock(buffer, 0, read, null, 0);
					at += read;
					if (show) { long a = at; Ui(() => bar.Value = (int)Math.Min(1000, a * 1000 / Math.Max(1L, total))); }
				}
				sha.TransformFinalBlock(buffer, 0, 0);
				return BitConverter.ToString(sha.Hash).Replace("-", "").ToLowerInvariant();
			}
		}

		/** One piece of the whole Setup into its place. A dropped connection picks up where it got to, as long as it keeps getting somewhere. */
		void Fetch(SetupSource source, string part, long from, long length)
		{
			long got = 0;
			int failures = 0;
			var buffer = new byte[1 << 16];
			using (var to = new FileStream(part, FileMode.Open, FileAccess.Write, FileShare.ReadWrite))
			{
				while (got < length && !cancelled)
				{
					long before = got;
					try
					{
						using (Stream stream = source.Open(from + got, length - got))
						{
							to.Seek(from + got, SeekOrigin.Begin);
							int read;
							while (got < length && !cancelled && (read = stream.Read(buffer, 0, (int)Math.Min(buffer.Length, length - got))) > 0)
							{
								to.Write(buffer, 0, read);
								got += read;
								Interlocked.Add(ref received, read);
							}
							if (got < length && !cancelled) throw new IOException("a part of the Setup stopped early");
						}
					}
					catch (IOException e) { if (!TryAgain(e, got > before, ref failures)) throw; }
					catch (WebException e) { if (!TryAgain(e, got > before, ref failures)) throw; }
				}
			}
		}

		static bool TryAgain(Exception e, bool gotSomewhere, ref int failures)
		{
			failures = gotSomewhere ? 1 : failures + 1;
			Log("download interrupted (" + e.Message + "), try " + failures);
			if (failures >= 4) return false;
			Thread.Sleep(1000 * failures);
			return true;
		}

		/** Each piece on one of Connections threads, the bar showing what's come down; the first failure stops the rest and is thrown here. */
		void AllAtOnce(List<long> jobs, Action<long> job, long total)
		{
			int next = -1;
			Exception failed = null;
			var threads = new List<Thread>();
			for (int i = 0; i < Math.Min(Connections, jobs.Count); i++)
			{
				var thread = new Thread(() =>
				{
					try
					{
						while (!cancelled && Volatile.Read(ref failed) == null)
						{
							int j = Interlocked.Increment(ref next);
							if (j >= jobs.Count) break;
							job(jobs[j]);
						}
					}
					catch (Exception e) { Interlocked.CompareExchange(ref failed, e, null); }
				}) { IsBackground = true };
				threads.Add(thread);
				thread.Start();
			}
			var clock = Stopwatch.StartNew();
			foreach (Thread thread in threads)
				while (!thread.Join(250)) Show(total, clock);
			Show(total, clock);
			if (failed != null) throw failed;
		}

		void Show(long total, Stopwatch clock)
		{
			long d = Interlocked.Read(ref received);
			double rate = d / Math.Max(1.0, clock.Elapsed.TotalSeconds);
			string left = rate > 1 && d < total ? string.Format(", about {0} left", Minutes((total - d) / rate)) : "";
			Ui(() =>
			{
				label.Text = string.Format("Downloading Wildreach {0}...\n{1:N0} of {2:N0} MB{3}", Version, d >> 20, total >> 20, left);
				bar.Value = (int)Math.Min(1000, d * 1000 / Math.Max(1L, total));
			});
		}

		static string Minutes(double seconds)
		{
			if (seconds < 90) return "a minute";
			if (seconds < 3600) return Math.Round(seconds / 60) + " minutes";
			return string.Format("{0:0.#} hours", seconds / 3600);
		}
	}
}
