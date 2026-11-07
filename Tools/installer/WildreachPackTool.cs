// Makes Wildreach-Setup.exe from the installer program and the packaged game (Tools\make_installer.ps1 runs it):
//   WildreachPackTool <game folder> <installer program> <Setup to write> <version>
// The format is in WildreachPack.cs.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using WildreachPack;

static class WildreachPackTool
{
	static int Main(string[] args)
	{
		if (args.Length != 4) { Console.Error.WriteLine("usage: <game folder> <installer program> <Setup to write> <version>"); return 2; }
		string game = Path.GetFullPath(args[0]).TrimEnd('\\'), stub = args[1], setup = args[2];
		var index = new Index { Version = args[3] };
		var stored = new Dictionary<string, BlockRef>();
		var hasher = new SHA256Cng();
		long raw = 0, repeated = 0;
		File.Copy(stub, setup, true);
		using (var output = new FileStream(setup, FileMode.Append, FileAccess.Write, FileShare.None, 1 << 20))
		{
			// In a fixed order, so the same game makes the same Setup.
			foreach (string path in Directory.GetFiles(game, "*", SearchOption.AllDirectories).OrderBy(p => p, StringComparer.Ordinal))
			{
				string relative = path.Substring(game.Length + 1);
				if (!Index.IsSafePath(relative)) throw new InvalidDataException("this must not ship: " + relative);
				var file = new PackedFile { Path = relative, Size = new FileInfo(path).Length };
				using (var whole = new WholeHash())
				using (var input = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 4096))
				{
					Blocks.Cut(input, (buffer, start, length, offset) =>
					{
						whole.Add(buffer, start, length);
						string name = Blocks.Name(hasher, buffer, start, length);
						BlockRef block;
						if (stored.TryGetValue(name, out block)) repeated += length;
						else
						{
							byte[] packed = Blocks.Pack(buffer, start, length);
							bool keep = packed.Length < length;   // what didn't compress is stored as it is: Packed == Size says so
							block = new BlockRef { Name = name, Size = length, At = output.Position, Packed = keep ? packed.Length : length };
							if (keep) output.Write(packed, 0, packed.Length);
							else output.Write(buffer, start, length);
							stored.Add(name, block);
						}
						file.Blocks.Add(block);
					});
					file.Sha = whole.Finish();
				}
				raw += file.Size;
				index.Files.Add(file);
			}
			long indexAt = output.Position;
			byte[] saved = index.Save();
			output.Write(saved, 0, saved.Length);
			byte[] footer = Index.Footer(indexAt, saved.Length);
			output.Write(footer, 0, footer.Length);
			Console.WriteLine("{0} files, {1:N0} MB, {2:N0} blocks ({3:N0} MB repeated), index {4:N0} KB", index.Files.Count, raw >> 20, stored.Count, repeated >> 20, saved.Length >> 10);
		}
		return 0;
	}
}
