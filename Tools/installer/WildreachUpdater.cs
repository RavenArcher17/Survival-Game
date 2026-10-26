// The Wildreach shortcut: checks GitHub for a newer release, and if there is one downloads its Wildreach-Setup.exe,
// installs it over this folder (saves are kept) and starts the game; otherwise (or offline) starts the game right away.
// Installed next to Wildreach.exe by Wildreach-Setup.exe; built by Tools\make_installer.ps1. The installed version is
// in version.txt beside it, a release's version is its tag ("v0.2.1").
//   WildreachUpdater.exe [game args]          the shortcut: check, update if needed, play
//   WildreachUpdater.exe --update --wait <pid> from the game's main menu (Update now): waits for the game to close first
//   --feed <url>   a stand-in for https://github.com/RavenArcher17/Survival-Game (Tools\updatertest.ps1)
//   --nolaunch     don't start the game afterwards (the test)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading;
using System.Windows.Forms;

static class WildreachUpdater
{
	const string Repo = "https://github.com/RavenArcher17/Survival-Game";
	static string dir, feed;
	static bool noLaunch;
	static List<string> gameArgs = new List<string>();

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
			else if (args[i] == "--wait" && i + 1 < args.Length) int.TryParse(args[++i], out waitPid);
			else if (args[i] == "--feed" && i + 1 < args.Length) feed = args[++i].TrimEnd('/');
			else gameArgs.Add(args[i]);
		}
		// The last update renamed this exe aside to replace it while it ran.
		try { foreach (string old in Directory.GetFiles(dir, "*.old")) File.Delete(old); } catch { }
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

	/** Progress window: download the new Setup, run it silently over this folder, then play. */
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

		void Work()
		{
			string temp = Path.Combine(Path.GetTempPath(), "Wildreach-Update");
			string setup = Path.Combine(temp, "Wildreach-Setup-" + latest + ".exe");
			try
			{
				Directory.CreateDirectory(temp);
				Download(feed + "/releases/download/" + Uri.EscapeDataString(latest) + "/Wildreach-Setup.exe", setup);
				if (cancelled) return;
				Ui(() => { label.Text = "Installing Wildreach " + latest.TrimStart('v') + "..."; bar.Style = ProgressBarStyle.Marquee; skip.Enabled = false; });
				using (Process p = Process.Start(new ProcessStartInfo(setup, "--silent --update --nolaunch --dir \"" + dir + "\"") { UseShellExecute = false, CreateNoWindow = true }))
				{
					p.WaitForExit();
					if (p.ExitCode != 0) throw new Exception("the installer stopped with code " + p.ExitCode);
				}
				try { File.Delete(setup); } catch { }
				Log("updated to " + latest);
				Ui(() => { Close(); Launch(); });
			}
			catch (Exception e)
			{
				Log("update failed: " + e.Message);
				exitCode = 1;
				Ui(() =>
				{
					MessageBox.Show(this, "Couldn't update Wildreach:\n" + e.Message + "\n\nThe game will start as it is; it tries again next time.", "Wildreach update");
					Close();
					Launch();
				});
			}
		}

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
