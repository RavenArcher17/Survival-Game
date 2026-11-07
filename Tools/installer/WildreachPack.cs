// What Wildreach-Setup.exe is made of, shared by the tool that makes it (WildreachPackTool.cs), the installer
// (WildreachSetup.cs) and the updater (WildreachUpdater.cs), so that all three cut files into the same blocks.
//
//   [the installer program][compressed blocks][the index, compressed][footer]
//
// Every file of the game is cut into blocks at places chosen by the bytes themselves (content-defined chunking), so a
// change in the middle of a file only changes the blocks around it: the ones after it still cut in the same places and
// come out the same even though they've moved. A block that appears twice is stored once. The index lists each file's
// size and hash, and its blocks in order with where each one is in the Setup.
//
// So one file does both jobs. A new player downloads the whole Setup and it unpacks itself. An installed copy reads the
// footer and the index with HTTP range requests, cuts its own files the same way to see which blocks it already has, and
// fetches only the others from the very same file. A release is still one file on GitHub, and if anything about the
// small update goes wrong, the whole Setup is right there.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace WildreachPack
{
	static class Blocks
	{
		public const int MinSize = 64 * 1024, MaxSize = 1024 * 1024;
		/** Top bits of the rolling hash: a cut where they're all zero, so blocks average about 2^AverageBits past the minimum. */
		public static int AverageBits = 18;

		static readonly ulong[] Gear = MakeGear();
		static ulong[] MakeGear()
		{
			// A fixed table: the tool, the installer and every updater ever shipped must cut the same way.
			var t = new ulong[256];
			ulong s = 0x5772656163680001UL;   // SplitMix64 from "Wreach"
			for (int i = 0; i < 256; i++)
			{
				s += 0x9E3779B97F4A7C15UL;
				ulong z = s;
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
				t[i] = z ^ (z >> 31);
			}
			return t;
		}

		/** One at a time: a game has hundreds of tiny files, and a fresh 4 MB for each would be most of the work. */
		static readonly byte[] CutBuffer = new byte[MaxSize * 4];

		/** Cuts a stream into blocks: Found(buffer, start, length, offset in the stream). The buffer is only good during the call. */
		public static void Cut(Stream input, Action<byte[], int, int, long> found)
		{
			ulong mask = ((1UL << AverageBits) - 1) << (64 - AverageBits);
			byte[] buffer = CutBuffer;
			int have = 0, at = 0;
			long offset = 0;
			bool ended = false;
			ulong[] gear = Gear;
			while (true)
			{
				if (!ended && have - at < MaxSize)
				{
					Buffer.BlockCopy(buffer, at, buffer, 0, have - at);
					have -= at;
					at = 0;
					while (have < buffer.Length)
					{
						int read = input.Read(buffer, have, buffer.Length - have);
						if (read <= 0) { ended = true; break; }
						have += read;
					}
				}
				int left = have - at;
				if (left <= 0) return;
				int length = left;
				if (left > MinSize)
				{
					int stop = at + Math.Min(left, MaxSize);
					length = stop - at;
					ulong hash = 0;
					// (The hash forgets all but the last 64 bytes, so it needs only those before the first place it can cut.)
					for (int i = at + MinSize - 64; i < stop; i++)
					{
						hash = (hash << 1) + gear[buffer[i]];
						if ((hash & mask) == 0 && i >= at + MinSize - 1)
						{
							length = i + 1 - at;
							break;
						}
					}
				}
				found(buffer, at, length, offset);
				at += length;
				offset += length;
			}
		}

		public static string Hex(byte[] bytes, int count)
		{
			var s = new StringBuilder(count * 2);
			for (int i = 0; i < count; i++) s.Append(bytes[i].ToString("x2"));
			return s.ToString();
		}

		/** A block's name: the first 16 bytes of its SHA-256. */
		public static string Name(SHA256 sha, byte[] buffer, int start, int length)
		{
			return Hex(sha.ComputeHash(buffer, start, length), 16);
		}

		public static byte[] Pack(byte[] buffer, int start, int length)
		{
			using (var packed = new MemoryStream())
			{
				using (var deflate = new DeflateStream(packed, CompressionLevel.Optimal, true)) deflate.Write(buffer, start, length);
				return packed.ToArray();
			}
		}

		/** A block as it's stored back to its bytes. Stored == Size means it's raw: it didn't compress. */
		public static byte[] Unpack(byte[] stored, int start, int packedLength, int size)
		{
			var bytes = new byte[size];
			if (packedLength == size)
			{
				Buffer.BlockCopy(stored, start, bytes, 0, size);
				return bytes;
			}
			using (var from = new MemoryStream(stored, start, packedLength))
			using (var deflate = new DeflateStream(from, CompressionMode.Decompress))
			{
				int got = 0, read;
				while (got < size && (read = deflate.Read(bytes, got, size - got)) > 0) got += read;
				if (got != size) throw new InvalidDataException("a block came out the wrong size");
			}
			return bytes;
		}
	}

	/** A whole file's SHA-256, fed a piece at a time. */
	class WholeHash : IDisposable
	{
		readonly SHA256 sha = new SHA256Cng();
		public void Add(byte[] buffer, int start, int length) { sha.TransformBlock(buffer, start, length, null, 0); }
		public string Finish()
		{
			sha.TransformFinalBlock(new byte[0], 0, 0);
			return Blocks.Hex(sha.Hash, 32);
		}
		public void Dispose() { sha.Dispose(); }
	}

	class BlockRef
	{
		public string Name;
		public int Size, Packed;
		public long At;   // where its stored bytes are in the Setup
	}

	class PackedFile
	{
		public string Path, Sha;
		public long Size;
		public readonly List<BlockRef> Blocks = new List<BlockRef>();
	}

	class Index
	{
		public const string Magic = "WRPACK01";
		public const int FooterSize = 24;   // the index's place, its stored length, the magic
		public string Version = "";
		public readonly List<PackedFile> Files = new List<PackedFile>();

		public byte[] Save()
		{
			var text = new StringBuilder();
			text.Append("wildreach-pack 1\nversion ").Append(Version).Append('\n');
			foreach (PackedFile f in Files)
			{
				text.Append("F\t").Append(f.Sha).Append('\t').Append(f.Size).Append('\t').Append(f.Path).Append('\n');
				foreach (BlockRef b in f.Blocks) text.Append("B\t").Append(b.Name).Append('\t').Append(b.Size).Append('\t').Append(b.At).Append('\t').Append(b.Packed).Append('\n');
			}
			byte[] raw = Encoding.UTF8.GetBytes(text.ToString());
			using (var packed = new MemoryStream())
			{
				using (var deflate = new DeflateStream(packed, CompressionLevel.Optimal, true)) deflate.Write(raw, 0, raw.Length);
				return packed.ToArray();
			}
		}

		public static Index Load(byte[] stored)
		{
			string text;
			using (var from = new MemoryStream(stored))
			using (var deflate = new DeflateStream(from, CompressionMode.Decompress))
			using (var reader = new StreamReader(deflate, Encoding.UTF8)) text = reader.ReadToEnd();
			string[] lines = text.Split('\n');
			if (lines.Length < 2 || lines[0] != "wildreach-pack 1" || !lines[1].StartsWith("version ")) throw new InvalidDataException("not an index this updater can read");
			var index = new Index { Version = lines[1].Substring(8) };
			PackedFile file = null;
			for (int i = 2; i < lines.Length; i++)
			{
				if (lines[i].Length == 0) continue;
				string[] p = lines[i].Split('\t');
				if (p[0] == "F" && p.Length == 4)
				{
					if (!IsSafePath(p[3])) throw new InvalidDataException("the index has a file it mustn't touch: " + p[3]);
					file = new PackedFile { Sha = p[1], Size = long.Parse(p[2]), Path = p[3] };
					index.Files.Add(file);
				}
				else if (p[0] == "B" && p.Length == 5 && file != null)
				{
					file.Blocks.Add(new BlockRef { Name = p[1], Size = int.Parse(p[2]), At = long.Parse(p[3]), Packed = int.Parse(p[4]) });
				}
				else throw new InvalidDataException("the index has a line it can't read");
			}
			if (index.Files.Count == 0) throw new InvalidDataException("the index lists no files");
			return index;
		}

		/** Inside the game's folder, and never a player's saves or settings. */
		public static bool IsSafePath(string path)
		{
			if (string.IsNullOrEmpty(path) || System.IO.Path.IsPathRooted(path) || path.IndexOf(':') >= 0) return false;
			string[] parts = path.Replace('/', '\\').Split('\\');
			if (parts.Any(s => s == ".." || s == "." || s.Length == 0)) return false;
			for (int i = 0; i + 1 < parts.Length; i++) if (parts[i].Equals("Saved", StringComparison.OrdinalIgnoreCase)) return false;
			return true;
		}

		public static byte[] Footer(long indexAt, long indexLength)
		{
			var footer = new byte[FooterSize];
			Buffer.BlockCopy(BitConverter.GetBytes(indexAt), 0, footer, 0, 8);
			Buffer.BlockCopy(BitConverter.GetBytes(indexLength), 0, footer, 8, 8);
			Buffer.BlockCopy(Encoding.ASCII.GetBytes(Magic), 0, footer, 16, 8);
			return footer;
		}

		/** False: not a Setup of this kind (one from before these updates). */
		public static bool ReadFooter(byte[] footer, out long indexAt, out long indexLength)
		{
			indexAt = indexLength = 0;
			if (footer == null || footer.Length != FooterSize || Encoding.ASCII.GetString(footer, 16, 8) != Magic) return false;
			indexAt = BitConverter.ToInt64(footer, 0);
			indexLength = BitConverter.ToInt64(footer, 8);
			return true;
		}

		public const string InstalledList = "installed-files.txt";

		/**
		 * Removes what the last version installed that this one doesn't have, then records this one's list. Only files on
		 * the last list go: a folder with no list (installed before these updates) keeps everything, and nothing a player
		 * made is ever on a list.
		 */
		public void RemoveStaleAndRecord(string dir)
		{
			string list = System.IO.Path.Combine(dir, InstalledList);
			var now = new HashSet<string>(Files.Select(f => f.Path), StringComparer.OrdinalIgnoreCase);
			if (File.Exists(list))
			{
				foreach (string line in File.ReadAllLines(list))
				{
					string old = line.Trim();
					if (old.Length == 0 || now.Contains(old) || !IsSafePath(old)) continue;
					try
					{
						string path = System.IO.Path.Combine(dir, old);
						if (File.Exists(path)) File.Delete(path);
					}
					catch { /* next time */ }
				}
			}
			File.WriteAllLines(list, Files.Select(f => f.Path));
		}
	}
}
