// The Wildreach shortcut: checks GitHub for a newer release, and if there is one brings this folder up to it (saves are
// kept) and starts the game; otherwise (or offline) starts the game right away. It fetches only what changed, from the
// release's Wildreach-Setup.exe (WildreachPack.cs); if that can't be done it downloads the whole Setup and runs it.
// Installed next to Wildreach.exe by Wildreach-Setup.exe; built by Tools\make_installer.ps1. The installed version is
// in version.txt beside it, a release's version is its tag ("v0.2.1").
//   WildreachUpdater.exe [game args]          the shortcut: check, update if needed, play
//   WildreachUpdater.exe --update --wait <pid> from the game's main menu (Update now): waits for the game to close first
//   --feed <url>   a stand-in for https://github.com/RavenArcher17/Survival-Game (Tools\updatertest.ps1)
//   --nolaunch     don't start the game afterwards (the test)
//   --quiet        log a failure instead of showing a message about it (the test)
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

static class WildreachUpdater
{
	const string Repo = "https://github.com/RavenArcher17/Survival-Game";
	static string dir, feed;
	static bool noLaunch, quiet;
	static List<string> gameArgs = new List<string>();

	/** A small update can't finish, and neither could the whole Setup: say why and play as it is. */
	class CantUpdate : Exception { public CantUpdate(string why) : base(why) { } }

	[STAThread]
	static int Main(string[] args)
	{
		dir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
		feed = Repo;
		bool fromGame = false;
		int waitPid = 0;
		for (int i = 0; i < args.Length; i++)
		{
			if (args[i] == "--update") fromGame = true;
			else if (args[i] == "--nolaunch") noLaunch = true;
			else if (args[i] == "--quiet") quiet = true;
			else if (args[i] == "--wait" && i + 1 < args.Length) int.TryParse(args[++i], out waitPid);
			else if (args[i] == "--feed" && i + 1 < args.Length) feed = args[++i].TrimEnd('/');
			else gameArgs.Add(args[i]);
		}
		// The last update renamed this exe aside to replace it while it ran.
		try { foreach (string old in Directory.GetFiles(dir, "*.old")) File.Delete(old); } catch { }
		// Whole Setups from earlier updates: 1.7 GB each, and deleting one straight after it runs often fails (a virus scanner
		// is still reading it, say), so they piled up. Nothing's using them now.
		try
		{
			foreach (string left in Directory.GetFiles(Path.Combine(Path.GetTempPath(), "Wildreach-Update")))
			{
				try { File.Delete(left); } catch { }
			}
		}
		catch { }
		ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

		string installed = Installed();
		string latest = LatestTag(fromGame ? 15000 : 5000);
		Log("installed " + installed + ", latest " + (latest ?? "unknown"));
		if (latest == null || !Newer(latest, installed))
		{
			if (fromGame) MessageBox.Show(latest == null ? "Couldn't reach GitHub to check for updates." : "Wildreach is up to date (" + installed + ").", "Wildreach");
			Launch();
			return 0;
		}
		if (waitPid > 0)
		{
			try { Process.GetProcessById(waitPid).WaitForExit(30000); } catch { }
		}
		Application.EnableVisualStyles();
		return new UpdateForm(installed, latest).Run();
	}

	static string Installed()
	{
		try { return File.ReadAllText(Path.Combine(dir, "version.txt")).Trim(); } catch { return "0"; }
	}

	/** The newest release's tag: github.com/.../releases/latest redirects to .../releases/tag/<tag> (no API limit). */
	static string LatestTag(int timeoutMs)
	{
		try
		{
			var request = (HttpWebRequest)WebRequest.Create(feed + "/releases/latest");
			request.AllowAutoRedirect = false;
			request.Timeout = timeoutMs;
			request.UserAgent = "WildreachUpdater";
			using (var response = (HttpWebResponse)request.GetResponse())
			{
				string location = response.Headers["Location"] ?? "";
				int at = location.LastIndexOf("/tag/", StringComparison.Ordinal);
				return at >= 0 ? Uri.UnescapeDataString(location.Substring(at + 5)) : null;
			}
		}
		catch (Exception e) { Log("check failed: " + e.Message); return null; }
	}

	/** "v0.2.1" is newer than "0.2.0"; a tag that isn't a version number never is. */
	static bool Newer(string tag, string installed)
	{
		int[] a = Parse(tag), b = Parse(installed);
		if (a == null) return false;
		if (b == null) return true;
		for (int i = 0; i < Math.Max(a.Length, b.Length); i++)
		{
			int x = i < a.Length ? a[i] : 0, y = i < b.Length ? b[i] : 0;
			if (x != y) return x > y;
		}
		return false;
	}

	static int[] Parse(string version)
	{
		string[] parts = version.Trim().TrimStart('v', 'V').Split('.');
		var numbers = new int[parts.Length];
		for (int i = 0; i < parts.Length; i++) if (!int.TryParse(parts[i], out numbers[i])) return null;
		return numbers;
	}

	static void Launch()
	{
		if (noLaunch) return;
		string args = string.Join(" ", gameArgs.Select(a => a.Contains(" ") ? "\"" + a + "\"" : a));
		Process.Start(new ProcessStartInfo(Path.Combine(dir, "Wildreach.exe"), args) { WorkingDirectory = dir, UseShellExecute = true });
	}

	static void Log(string line)
	{
		try { File.AppendAllText(Path.Combine(dir, "updater.log"), DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss ") + line + "\r\n"); } catch { }
	}

	/** Progress window: fetch what changed (or the whole Setup and run it silently over this folder), then play. */
	class UpdateForm : Form
	{
		readonly string installed, latest;
		readonly Label label = new Label { Left = 16, Top = 14, Width = 470, Height = 44 };
		readonly ProgressBar bar = new ProgressBar { Left = 16, Top = 62, Width = 470, Height = 22, Maximum = 1000 };
		readonly Button skip = new Button { Left = 316, Top = 96, Width = 170, Height = 28, Text = "Play without updating" };
		volatile bool cancelled;
		int exitCode;

		public UpdateForm(string installed, string latest)
		{
			this.installed = installed;
			this.latest = latest;
			Text = "Wildreach update";
			Width = 520; Height = 178;
			FormBorderStyle = FormBorderStyle.FixedDialog;
			MaximizeBox = false; MinimizeBox = true;
			StartPosition = FormStartPosition.CenterScreen;
			try { Icon = Icon.ExtractAssociatedIcon(Path.Combine(dir, "Wildreach.exe")); } catch { }
			label.Text = "Wildreach " + latest.TrimStart('v') + " is out (you have " + installed + "). Downloading it...";
			Controls.Add(label); Controls.Add(bar); Controls.Add(skip);
			skip.Click += (s, e) => { cancelled = true; Log("skipped by the player"); Close(); Launch(); };
			Shown += (s, e) => new Thread(Work) { IsBackground = true }.Start();
		}

		public int Run() { Application.Run(this); return exitCode; }

		void Ui(Action a) { try { if (!IsDisposed) BeginInvoke(a); } catch { } }

		string SetupUrl { get { return feed + "/releases/download/" + Uri.EscapeDataString(latest) + "/Wildreach-Setup.exe"; } }

		void Work()
		{
			string temp = Path.Combine(Path.GetTempPath(), "Wildreach-Update");
			string setup = Path.Combine(temp, "Wildreach-Setup-" + latest + ".exe");
			try
			{
				// Just what changed, if it can be done. Whatever goes wrong with that, the whole Setup is still there.
				try
				{
					if (SmallUpdate())
					{
						Ui(() => { Close(); Launch(); });
						return;
					}
				}
				catch (CantUpdate) { throw; }
				catch (Exception e) { Log("small update didn't work (" + e.Message + "): downloading the whole game instead"); }
				if (cancelled) return;
				Ui(() => { skip.Enabled = true; bar.Style = ProgressBarStyle.Blocks; bar.Value = 0; label.Text = "Wildreach " + latest.TrimStart('v') + " is out (you have " + installed + "). Downloading it..."; });
				Directory.CreateDirectory(temp);
				Download(SetupUrl, setup);
				if (cancelled) return;
				Ui(() => { label.Text = "Installing Wildreach " + latest.TrimStart('v') + "..."; bar.Style = ProgressBarStyle.Marquee; skip.Enabled = false; });
				using (Process p = Process.Start(new ProcessStartInfo(setup, "--silent --update --nolaunch --dir \"" + dir + "\"") { UseShellExecute = false, CreateNoWindow = true }))
				{
					p.WaitForExit();
					if (p.ExitCode != 0) throw new Exception("the installer stopped with code " + p.ExitCode);
				}
				try { File.Delete(setup); } catch { }
				try { Directory.Delete(Path.Combine(dir, WorkFolder), true); } catch { }   // what a small update left when it gave up
				Log("updated to " + latest);
				Ui(() => { Close(); Launch(); });
			}
			catch (Exception e)
			{
				Log("update failed: " + e.Message);
				exitCode = 1;
				Ui(() =>
				{
					if (!quiet) MessageBox.Show(this, "Couldn't update Wildreach:\n" + e.Message + "\n\nThe game will start as it is; it tries again next time.", "Wildreach update");
					Close();
					Launch();
				});
			}
		}

		// ---------------------------------------------------------------- the small way

		const string WorkFolder = ".update";
		/** Blocks closer together than this in the Setup come down in one request (fewer requests beat a few wasted bytes). */
		const long JoinGap = 256 * 1024;
		/** Past this share of the Setup, the whole Setup is as quick and simpler. */
		const double WorthItShare = 0.8;

		class Have { public string Path; public long At; public int Size; }

		/** True: this folder is now the new version. False: not possible this way (or the player chose to play on). */
		bool SmallUpdate()
		{
			var clock = Stopwatch.StartNew();
			// The index, from the end of the Setup. (GitHub won't serve "the last N bytes", so first ask how long the file is.)
			long total;
			Range(SetupUrl, 0, 1, out total);
			long unused, indexAt, indexLength;
			if (!Index.ReadFooter(Range(SetupUrl, total - Index.FooterSize, Index.FooterSize, out unused), out indexAt, out indexLength))
			{
				Log("that Setup has no index to update from");
				return false;
			}
			Index index = Index.Load(Range(SetupUrl, indexAt, indexLength, out unused));

			// What this copy has: each file cut the same way. A file whose hash already matches needs nothing.
			Say("Checking your files...", 0, 0);
			var have = new Dictionary<string, Have>();
			var changed = new List<PackedFile>();
			long scanTotal = Math.Max(1L, index.Files.Sum(f => f.Size)), scanned = 0;
			using (SHA256 hasher = new SHA256Cng())
			{
				foreach (PackedFile file in index.Files)
				{
					if (cancelled) return false;
					if (!Scan(file, have, hasher)) changed.Add(file);
					scanned += file.Size;
					long s = scanned;
					Ui(() => bar.Value = (int)Math.Min(1000, s * 1000 / scanTotal));
				}
			}

			// What it hasn't: each block once, in the order they're in the Setup.
			var missing = new Dictionary<string, BlockRef>();
			foreach (PackedFile file in changed)
				foreach (BlockRef block in file.Blocks)
					if (!have.ContainsKey(block.Name)) missing[block.Name] = block;
			long download = missing.Values.Sum(b => (long)b.Packed);
			long rebuilt = changed.Sum(f => f.Size);
			Log(string.Format("small update: {0} of {1} files changed, {2:N0} KB to download of {3:N0} KB", changed.Count, index.Files.Count, download >> 10, total >> 10));
			if (download >= total * WorthItShare) return false;
			long free = new DriveInfo(Path.GetPathRoot(dir)).AvailableFreeSpace;
			if (free < download + rebuilt + (64L << 20)) throw new CantUpdate(string.Format("It needs {0:N0} MB of free disk space and there's {1:N0} MB.", (download + rebuilt + (64L << 20)) >> 20, free >> 20));

			string work = Path.Combine(dir, WorkFolder), blocks = Path.Combine(work, "blocks"), built = Path.Combine(work, "new"), aside = Path.Combine(work, "old");
			// What an interrupted try already fetched stays good for the same version, and is no use for any other.
			string forTag = Path.Combine(work, "tag.txt");
			if (Directory.Exists(work) && (!File.Exists(forTag) || File.ReadAllText(forTag).Trim() != latest)) Directory.Delete(work, true);
			Directory.CreateDirectory(blocks);
			File.WriteAllText(forTag, latest);
			Fetch(missing, blocks, download);
			if (cancelled) return false;

			// Build every new file beside the old ones, each checked against the index, before touching any of them.
			Ui(() => { label.Text = "Installing Wildreach " + latest.TrimStart('v') + "..."; bar.Value = 0; });
			if (Directory.Exists(built)) Directory.Delete(built, true);
			long builtBytes = 0, builtTotal = Math.Max(1L, rebuilt);
			foreach (PackedFile file in changed)
			{
				if (cancelled) return false;
				Build(file, have, blocks, Path.Combine(built, file.Path));
				builtBytes += file.Size;
				long b = builtBytes;
				Ui(() => bar.Value = (int)Math.Min(1000, b * 1000 / builtTotal));
			}

			Ui(() => { skip.Enabled = false; bar.Style = ProgressBarStyle.Marquee; });
			Swap(changed, built, aside);
			try { index.RemoveStaleAndRecord(dir); } catch (Exception e) { Log("couldn't tidy old files: " + e.Message); }
			try { Directory.Delete(work, true); } catch { }
			Log(string.Format("updated to {0}, fetching {1:N0} KB ({2:N1} s)", latest, download >> 10, clock.Elapsed.TotalSeconds));
			return true;
		}

		void Say(string text, long done, long total)
		{
			string line = "Wildreach " + latest.TrimStart('v') + " is out (you have " + installed + "). " + text;
			if (total > 0) line += string.Format("\n{0:N0} of {1:N0} MB", done >> 20, total >> 20);
			Ui(() => label.Text = line);
		}

		/** Cuts this copy's file (if it has it) into blocks; true if it's already what the index wants. */
		bool Scan(PackedFile file, Dictionary<string, Have> have, SHA256 hasher)
		{
			string path = Path.Combine(dir, file.Path);
			if (!File.Exists(path)) return false;
			try
			{
				using (var whole = new WholeHash())
				using (var input = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 4096))
				{
					long size = input.Length;
					Blocks.Cut(input, (buffer, start, length, offset) =>
					{
						whole.Add(buffer, start, length);
						string name = Blocks.Name(hasher, buffer, start, length);
						if (!have.ContainsKey(name)) have.Add(name, new Have { Path = path, At = offset, Size = length });
					});
					return whole.Finish() == file.Sha && size == file.Size;
				}
			}
			catch (IOException e)
			{
				// Can't be read (held open by something): it counts as different, and replacing it will say why it can't.
				Log("couldn't read " + file.Path + ": " + e.Message);
				return false;
			}
			catch (UnauthorizedAccessException e)
			{
				Log("couldn't read " + file.Path + ": " + e.Message);
				return false;
			}
		}

		/** Exact bytes of the Setup. Throws if the server doesn't give exactly those. */
		byte[] Range(string url, long from, long length, out long total)
		{
			var request = RangeRequest(url, from, length);
			using (var response = (HttpWebResponse)request.GetResponse())
			{
				total = CheckRange(response, from);
				var bytes = new byte[length];
				using (Stream stream = response.GetResponseStream())
				{
					int got = 0, read;
					while (got < length && (read = stream.Read(bytes, got, (int)length - got)) > 0) got += read;
					if (got != length) throw new IOException("a part of the Setup stopped early");
				}
				return bytes;
			}
		}

		static HttpWebRequest RangeRequest(string url, long from, long length)
		{
			var request = (HttpWebRequest)WebRequest.Create(url);
			request.UserAgent = "WildreachUpdater";
			request.Timeout = 30000;
			request.ReadWriteTimeout = 60000;
			request.AddRange(from, from + length - 1);
			return request;
		}

		/** The server must answer with the part asked for: anything else would be read as the wrong bytes. Returns the file's whole length. */
		static long CheckRange(HttpWebResponse response, long from)
		{
			string range = response.Headers["Content-Range"] ?? "";
			if (response.StatusCode != HttpStatusCode.PartialContent || !range.StartsWith("bytes " + from + "-")) throw new IOException("the server doesn't do partial downloads");
			long total;
			if (!long.TryParse(range.Substring(range.LastIndexOf('/') + 1), out total)) throw new IOException("the server's partial download makes no sense");
			return total;
		}

		/** Each missing block into its own file, as stored. What's already there from an interrupted try isn't fetched again. */
		void Fetch(Dictionary<string, BlockRef> missing, string folder, long total)
		{
			long done = 0;
			var pending = new List<BlockRef>();
			foreach (BlockRef block in missing.Values.OrderBy(b => b.At))
			{
				string file = Path.Combine(folder, block.Name);
				if (File.Exists(file) && new FileInfo(file).Length == block.Packed) done += block.Packed;
				else pending.Add(block);
			}
			Say("Downloading what's changed...", done, total);
			int next = 0, failures = 0;
			while (next < pending.Count && !cancelled)
			{
				// A run of blocks close together: one request.
				int last = next;
				while (last + 1 < pending.Count && pending[last + 1].At - (pending[last].At + pending[last].Packed) <= JoinGap) last++;
				long from = pending[next].At, length = pending[last].At + pending[last].Packed - from;
				int before = next;
				try
				{
					using (var response = (HttpWebResponse)RangeRequest(SetupUrl, from, length).GetResponse())
					using (Stream stream = response.GetResponseStream())
					{
						CheckRange(response, from);
						var clock = Stopwatch.StartNew();
						long position = from;
						var buffer = new byte[Blocks.MaxSize];
						for (; next <= last && !cancelled; next++)
						{
							BlockRef block = pending[next];
							for (long skip_ = block.At - position; skip_ > 0;)
							{
								int read = stream.Read(buffer, 0, (int)Math.Min(skip_, buffer.Length));
								if (read <= 0) throw new IOException("a part of the Setup stopped early");
								skip_ -= read;
							}
							int got = 0;
							while (got < block.Packed)
							{
								int read = stream.Read(buffer, got, block.Packed - got);
								if (read <= 0) throw new IOException("a part of the Setup stopped early");
								got += read;
							}
							// Checked as it arrives, so a bad byte can't reach the game. (A bad block isn't worth retrying: the whole Setup can't be right either way.)
							using (SHA256 hasher = new SHA256Cng())
							{
								byte[] bytes = Blocks.Unpack(buffer, 0, block.Packed, block.Size);
								if (Blocks.Name(hasher, bytes, 0, bytes.Length) != block.Name) throw new InvalidDataException("a block came down wrong");
							}
							using (var to = new FileStream(Path.Combine(folder, block.Name), FileMode.Create, FileAccess.Write)) to.Write(buffer, 0, block.Packed);
							position = block.At + block.Packed;
							done += block.Packed;
							if (clock.ElapsedMilliseconds > 200)
							{
								clock.Restart();
								long d = done;
								Say("Downloading what's changed...", d, total);
								Ui(() => bar.Value = (int)Math.Min(1000, d * 1000 / Math.Max(1L, total)));
							}
						}
					}
				}
				catch (IOException e) { if (!TryAgain(e, next > before, ref failures)) throw; }
				catch (WebException e) { if (!TryAgain(e, next > before, ref failures)) throw; }
			}
		}

		/** A dropped connection picks up at the block it was on, as long as it keeps getting somewhere. */
		static bool TryAgain(Exception e, bool gotSomewhere, ref int failures)
		{
			failures = gotSomewhere ? 1 : failures + 1;
			Log("download interrupted (" + e.Message + "), try " + failures);
			if (failures >= 4) return false;
			Thread.Sleep(1000 * failures);
			return true;
		}

		/** A new file beside the old one: blocks from wherever this copy has them, or fetched ones; its hash must match. */
		void Build(PackedFile file, Dictionary<string, Have> have, string blocks, string target)
		{
			Directory.CreateDirectory(Path.GetDirectoryName(target));
			var sources = new Dictionary<string, FileStream>();
			try
			{
				using (var output = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16))
				using (var whole = new WholeHash())
				{
					foreach (BlockRef block in file.Blocks)
					{
						byte[] bytes;
						Have local;
						if (have.TryGetValue(block.Name, out local))
						{
							FileStream source;
							if (!sources.TryGetValue(local.Path, out source))
							{
								source = new FileStream(local.Path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 4096);
								sources.Add(local.Path, source);
							}
							bytes = new byte[local.Size];
							source.Seek(local.At, SeekOrigin.Begin);
							int got = 0, read;
							while (got < bytes.Length && (read = source.Read(bytes, got, bytes.Length - got)) > 0) got += read;
							if (got != bytes.Length) throw new IOException(local.Path + " got shorter");
						}
						else
						{
							byte[] stored = File.ReadAllBytes(Path.Combine(blocks, block.Name));
							bytes = Blocks.Unpack(stored, 0, stored.Length, block.Size);
						}
						whole.Add(bytes, 0, bytes.Length);
						output.Write(bytes, 0, bytes.Length);
					}
					if (whole.Finish() != file.Sha) throw new InvalidDataException(file.Path + " came out wrong");
				}
			}
			finally { foreach (FileStream s in sources.Values) s.Dispose(); }
		}

		/**
		 * Puts the new files in place, the version number last, so a copy that stops part way still says it's the old version
		 * and the next start finishes the job. A file that can't be moved (the game's still using it) puts everything back.
		 */
		void Swap(List<PackedFile> changed, string built, string aside)
		{
			string self = Path.Combine(dir, "WildreachUpdater.exe");
			var done = new List<KeyValuePair<string, string>>();   // target, where the old one went (null: there wasn't one)
			try
			{
				foreach (PackedFile file in changed.OrderBy(f => f.Path.Equals("version.txt", StringComparison.OrdinalIgnoreCase) ? 1 : 0))
				{
					string target = Path.Combine(dir, file.Path), old = null;
					Directory.CreateDirectory(Path.GetDirectoryName(target));
					if (File.Exists(target))
					{
						// This program is running, so it can't be deleted yet: beside itself as .old, for the next start to clear.
						old = target.Equals(self, StringComparison.OrdinalIgnoreCase) ? target + ".old" : Path.Combine(aside, file.Path);
						Directory.CreateDirectory(Path.GetDirectoryName(old));
						if (File.Exists(old)) File.Delete(old);
						File.Move(target, old);
					}
					done.Add(new KeyValuePair<string, string>(target, old));
					File.Move(Path.Combine(built, file.Path), target);
				}
			}
			catch (IOException e) { PutBack(done, e); }
			catch (UnauthorizedAccessException e) { PutBack(done, e); }
		}

		static void PutBack(List<KeyValuePair<string, string>> done, Exception why)
		{
			for (int i = done.Count - 1; i >= 0; i--)
			{
				try
				{
					if (File.Exists(done[i].Key)) File.Delete(done[i].Key);
					if (done[i].Value != null) File.Move(done[i].Value, done[i].Key);
				}
				catch (Exception undo) { Log("couldn't put back " + done[i].Key + ": " + undo.Message); }
			}
			Log("couldn't replace a file: " + why.Message);
			throw new CantUpdate("A game file is in use. Close Wildreach and start it again.");
		}

		// ---------------------------------------------------------------- the whole Setup

		void Download(string url, string path)
		{
			string part = path + ".part";
			var request = (HttpWebRequest)WebRequest.Create(url);
			request.UserAgent = "WildreachUpdater";
			request.Timeout = 30000;
			request.ReadWriteTimeout = 60000;
			using (var response = (HttpWebResponse)request.GetResponse())
			using (Stream from = response.GetResponseStream())
			using (var to = new FileStream(part, FileMode.Create, FileAccess.Write))
			{
				long total = response.ContentLength, done = 0;
				var buffer = new byte[1 << 20];
				var clock = Stopwatch.StartNew();
				int read;
				while (!cancelled && (read = from.Read(buffer, 0, buffer.Length)) > 0)
				{
					to.Write(buffer, 0, read);
					done += read;
					if (clock.ElapsedMilliseconds > 200 || done == total)
					{
						clock.Restart();
						long d = done;
						Ui(() =>
						{
							if (total > 0) bar.Value = (int)Math.Min(1000, d * 1000 / total);
							label.Text = string.Format("Wildreach {0} is out (you have {1}). Downloading it...\n{2:N0} of {3:N0} MB", latest.TrimStart('v'), installed, d >> 20, total > 0 ? total >> 20 : 0);
						});
					}
				}
				if (cancelled) return;
				if (total > 0 && done != total) throw new Exception("the download stopped early");
			}
			if (File.Exists(path)) File.Delete(path);
			File.Move(part, path);
		}
	}
}
