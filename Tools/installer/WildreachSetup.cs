// One-file installer for friends: double-click -> unpacks Wildreach into your user folder (no admin), adds
// Desktop + Start Menu shortcuts, installs Microsoft's VC++ runtime only if it's missing, then starts the game.
// Running a newer Setup updates in place and keeps saves. The shortcuts start WildreachUpdater.exe, which fetches and
// usually fetches just what changed, from this same file (WildreachPack.cs), or runs the whole Setup (--silent --update).
// Built by Tools\make_installer.ps1 with the C# compiler that ships with Windows; the packaged game is on the end of this
// program, in blocks, with an index and a footer after them.
//   Wildreach-Setup.exe                         normal install
//   Wildreach-Setup.exe --silent --dir <path> --nolaunch [--noshortcuts]   (used by the tests)
//   Wildreach-Setup.exe --silent --update --dir <path> --nolaunch          (used by the updater: shortcuts left as they are)
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
using WildreachPack;

static class WildreachSetup
{
	[STAThread]
	static int Main(string[] args)
	{
		bool silent = args.Contains("--silent"), noLaunch = args.Contains("--nolaunch");
		bool shortcuts = !args.Contains("--update") && !args.Contains("--noshortcuts");
		int dirArg = Array.IndexOf(args, "--dir");
		string dir = dirArg >= 0 && dirArg + 1 < args.Length ? args[dirArg + 1]
			: Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "Wildreach");
		if (silent)
		{
			try { Install(dir, null, null, !noLaunch, shortcuts); return 0; }
			catch (Exception e) { Console.Error.WriteLine(e); return 1; }
		}

		Application.EnableVisualStyles();
		var form = new Form { Text = "Wildreach setup", Width = 520, Height = 170, FormBorderStyle = FormBorderStyle.FixedDialog,
			MaximizeBox = false, MinimizeBox = false, StartPosition = FormStartPosition.CenterScreen };
		var label = new Label { Left = 16, Top = 16, Width = 470, Height = 40, Text = "Installing Wildreach to\n" + dir };
		var bar = new ProgressBar { Left = 16, Top = 64, Width = 470, Height = 24, Maximum = 1000 };
		form.Controls.Add(label);
		form.Controls.Add(bar);
		form.Shown += (s, e) => new Thread(() =>
		{
			try
			{
				Install(dir, p => form.BeginInvoke((Action)(() => bar.Value = Math.Min(1000, (int)(p * 1000)))),
					t => form.BeginInvoke((Action)(() => label.Text = t)), !noLaunch, shortcuts);
				form.BeginInvoke((Action)(() => form.Close()));
			}
			catch (Exception ex)
			{
				form.BeginInvoke((Action)(() => { MessageBox.Show(form, "Setup failed:\n" + ex.Message, "Wildreach setup"); form.Close(); }));
			}
		}) { IsBackground = true }.Start();
		Application.Run(form);
		return 0;
	}

	static void Install(string dir, Action<double> progress, Action<string> status, bool launch, bool shortcuts)
	{
		Directory.CreateDirectory(dir);
		Index index;
		using (var self = new FileStream(Assembly.GetExecutingAssembly().Location, FileMode.Open, FileAccess.Read, FileShare.Read))
		{
			index = ReadIndex(self);
			long done = 0, total = Math.Max(1L, index.Files.Sum(f => f.Size));
			string root = Path.GetFullPath(dir) + Path.DirectorySeparatorChar;
			foreach (PackedFile file in index.Files)
			{
				string target = Path.GetFullPath(Path.Combine(dir, file.Path));
				if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;  // never write outside the folder
				Directory.CreateDirectory(Path.GetDirectoryName(target));
				Extract(self, file, target);
				done += file.Size;
				if (progress != null) progress(done / (double)total);
			}
		}
		index.RemoveStaleAndRecord(dir);

		// Microsoft's C++ runtime: most gaming PCs have it; install it only when it's missing (this one asks for admin).
		string sys = Environment.GetFolderPath(Environment.SpecialFolder.System);
		string redist = Path.Combine(dir, "Redist", "vc_redist.x64.exe");
		if ((!File.Exists(Path.Combine(sys, "vcruntime140.dll")) || !File.Exists(Path.Combine(sys, "msvcp140.dll"))) && File.Exists(redist))
		{
			if (status != null) status("Installing the Microsoft Visual C++ runtime...");
			using (Process p = Process.Start(new ProcessStartInfo(redist, "/install /passive /norestart") { UseShellExecute = true })) { p.WaitForExit(); }
		}

		string exe = Path.Combine(dir, "Wildreach.exe");
		// The shortcuts go through the updater (it starts the game), so a new release installs itself.
		string updater = Path.Combine(dir, "WildreachUpdater.exe");
		string start = File.Exists(updater) ? updater : exe;
		string menu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "Wildreach");
		if (shortcuts)
		{
			Shortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Wildreach.lnk"), start, exe);
			Directory.CreateDirectory(menu);
			Shortcut(Path.Combine(menu, "Wildreach.lnk"), start, exe);
		}
		File.WriteAllText(Path.Combine(dir, "Uninstall Wildreach.cmd"),
			"@echo off\r\necho Removing Wildreach (including saved characters on this PC)...\r\npause\r\n" +
			"del \"%USERPROFILE%\\Desktop\\Wildreach.lnk\" 2>nul\r\nrmdir /s /q \"" + menu + "\"\r\n" +
			"cd /d \"%TEMP%\"\r\nrmdir /s /q \"" + dir + "\"\r\n");
		if (shortcuts) Shortcut(Path.Combine(menu, "Uninstall Wildreach.lnk"), Path.Combine(dir, "Uninstall Wildreach.cmd"), null);

		if (launch) Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = dir, UseShellExecute = true });
	}

	static Index ReadIndex(FileStream self)
	{
		var footer = new byte[Index.FooterSize];
		self.Seek(-Index.FooterSize, SeekOrigin.End);
		ReadAll(self, footer, footer.Length);
		long at, length;
		if (!Index.ReadFooter(footer, out at, out length)) throw new InvalidDataException("this Setup is damaged: its index is missing");
		var stored = new byte[length];
		self.Seek(at, SeekOrigin.Begin);
		ReadAll(self, stored, stored.Length);
		return Index.Load(stored);
	}

	static void ReadAll(Stream from, byte[] buffer, int count)
	{
		int got = 0, read;
		while (got < count && (read = from.Read(buffer, got, count - got)) > 0) got += read;
		if (got != count) throw new EndOfStreamException("this Setup is damaged: it ends early");
	}

	/** A file in use (the updater that started this Setup) can't be overwritten but can be renamed: set it aside as
	 *  .old (the updater deletes those on its next start) and write the new one in its place. */
	static void Extract(FileStream self, PackedFile file, string target)
	{
		FileStream output;
		try { output = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16); }
		catch (IOException)
		{
			string old = target + ".old";
			if (File.Exists(old)) File.Delete(old);
			File.Move(target, old);
			output = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16);
		}
		using (output)
		using (var whole = new WholeHash())
		{
			var stored = new byte[Blocks.MaxSize];
			foreach (BlockRef block in file.Blocks)
			{
				self.Seek(block.At, SeekOrigin.Begin);
				ReadAll(self, stored, block.Packed);
				byte[] bytes = Blocks.Unpack(stored, 0, block.Packed, block.Size);
				whole.Add(bytes, 0, bytes.Length);
				output.Write(bytes, 0, bytes.Length);
			}
			if (whole.Finish() != file.Sha) throw new InvalidDataException("this Setup is damaged: " + file.Path + " came out wrong");
		}
	}

	static void Shortcut(string lnk, string target, string icon)
	{
		try
		{
			Type shellType = Type.GetTypeFromProgID("WScript.Shell");
			dynamic shell = Activator.CreateInstance(shellType);
			dynamic link = shell.CreateShortcut(lnk);
			link.TargetPath = target;
			link.WorkingDirectory = Path.GetDirectoryName(target);
			if (icon != null) link.IconLocation = icon + ",0";
			link.Save();
		}
		catch { /* shortcuts are a convenience */ }
	}
}
