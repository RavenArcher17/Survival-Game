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
// fetches only the others from the very same file. If anything about the small update goes wrong, the whole Setup is
// right there. A Setup over GitHub's 2 GiB file limit goes up in parts (SetupParts, at the end), read as the same one file.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
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

	/**
	 * A Setup too big for one GitHub release file (GitHub takes nothing over 2 GiB) goes up cut into parts: Wildreach-Setup.001,
	 * .002... and this list, Wildreach-Setup.parts, beside them; Wildreach-Setup.exe is then the small download program
	 * (WildreachBootstrap.cs) that puts the parts back together and runs the whole Setup. Made by Tools\make_installer.ps1.
	 *   WRPARTS1
	 *   version 0.8.0
	 *   size <the whole Setup's length>
	 *   sha256 <the whole Setup's hash>
	 *   part Wildreach-Setup.001 <length> <hash>      (in order)
	 */
	class SetupParts
	{
		public const string ListName = "Wildreach-Setup.parts";
		public const string Magic = "WRPARTS1";
		public string Version, Sha;
		public long Size;
		public class Part { public string Name, Sha; public long Size, At; }
		public readonly List<Part> Parts = new List<Part>();

		public static SetupParts Parse(string text)
		{
			var lines = text.Replace("\r", "").Split('\n').Select(l => l.Trim()).Where(l => l.Length > 0).ToList();
			if (lines.Count < 5 || lines[0] != Magic) throw new InvalidDataException("not a Setup parts list");
			var list = new SetupParts();
			long at = 0;
			foreach (string line in lines.Skip(1))
			{
				string[] w = line.Split(' ');
				if (w[0] == "version" && w.Length == 2) list.Version = w[1];
				else if (w[0] == "size" && w.Length == 2) list.Size = long.Parse(w[1]);
				else if (w[0] == "sha256" && w.Length == 2) list.Sha = w[1].ToLowerInvariant();
				else if (w[0] == "part" && w.Length == 4)
				{
					// A part's name is a plain file name on the release, nothing that could reach anywhere else.
					if (w[1].Length == 0 || w[1].IndexOfAny(new[] { '/', '\\', '?', '#', '%', ':' }) >= 0 || w[1].Contains("..")) throw new InvalidDataException("a part name it won't use: " + w[1]);
					var part = new Part { Name = w[1], Size = long.Parse(w[2]), Sha = w[3].ToLowerInvariant(), At = at };
					if (part.Size <= 0) throw new InvalidDataException("an empty part");
					at += part.Size;
					list.Parts.Add(part);
				}
			}
			if (list.Parts.Count == 0 || list.Size != at || string.IsNullOrEmpty(list.Sha)) throw new InvalidDataException("the Setup parts list doesn't add up");
			return list;
		}
	}

	/**
	 * Where a release's whole Setup comes from: the one file Wildreach-Setup.exe, or its parts (SetupParts). Open() gives any
	 * stretch of the whole Setup's bytes either way, so the updater reads one big Setup whatever the release looks like.
	 */
	class SetupSource
	{
		public const string SetupName = "Wildreach-Setup.exe";
		public string Base;          // .../releases/download/<tag>
		public SetupParts Parts;     // null: the one file
		public long Length;
		public string UserAgent = "Wildreach";

		public string SetupUrl { get { return Base + "/" + SetupName; } }

		/** The release's parts list if it has one (else the one file, its length asked for). A network failure throws. */
		public static SetupSource Find(string releaseBase, string userAgent)
		{
			var source = new SetupSource { Base = releaseBase.TrimEnd('/'), UserAgent = userAgent };
			var request = (HttpWebRequest)WebRequest.Create(source.Base + "/" + SetupParts.ListName);
			request.UserAgent = userAgent;
			request.Timeout = 30000;
			request.ReadWriteTimeout = 30000;
			try
			{
				using (var response = (HttpWebResponse)request.GetResponse())
				using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8))
				{
					if (response.ContentLength > (1 << 20)) throw new InvalidDataException("the Setup parts list is too long");
					source.Parts = SetupParts.Parse(reader.ReadToEnd());
					source.Length = source.Parts.Size;
					return source;
				}
			}
			catch (WebException e)
			{
				var r = e.Response as HttpWebResponse;
				if (r == null || r.StatusCode != HttpStatusCode.NotFound) throw;
			}
			// No list: one file. Its length from a one-byte part (GitHub's store won't serve "the last N bytes").
			using (Stream one = source.OpenOne(source.SetupUrl, 0, 1, -1)) { }
			return source;
		}

		HttpWebRequest Request(string url, long from, long length)
		{
			var request = (HttpWebRequest)WebRequest.Create(url);
			request.UserAgent = UserAgent;
			request.Timeout = 30000;
			request.ReadWriteTimeout = 60000;
			request.AddRange(from, from + length - 1);
			return request;
		}

		/** One range of one file; the server must answer with exactly it (expected: the file's length, or -1 to learn it). */
		Stream OpenOne(string url, long from, long length, long expected)
		{
			var response = (HttpWebResponse)Request(url, from, length).GetResponse();
			try
			{
				string range = response.Headers["Content-Range"] ?? "";
				if (response.StatusCode != HttpStatusCode.PartialContent || !range.StartsWith("bytes " + from + "-")) throw new IOException("the server doesn't do partial downloads");
				long total;
				if (!long.TryParse(range.Substring(range.LastIndexOf('/') + 1), out total)) throw new IOException("the server's partial download makes no sense");
				if (expected < 0) Length = total;
				else if (total != expected) throw new IOException("a part of the Setup isn't the length its list says");
				return new ResponseStream(response, length);
			}
			catch { response.Dispose(); throw; }
		}

		/** Exactly the bytes [from, from + length) of the whole Setup, across parts when it's in parts. */
		public Stream Open(long from, long length)
		{
			if (from < 0 || length <= 0 || from + length > Length) throw new ArgumentOutOfRangeException("from", "outside the Setup");
			if (Parts == null) return OpenOne(SetupUrl, from, length, Length);
			return new Spanning(this, from, length);
		}

		/** Reads exactly length bytes from where Open starts. */
		public byte[] Read(long from, long length)
		{
			var bytes = new byte[length];
			using (Stream s = Open(from, length))
			{
				int got = 0, read;
				while (got < length && (read = s.Read(bytes, got, (int)length - got)) > 0) got += read;
				if (got != length) throw new IOException("a part of the Setup stopped early");
			}
			return bytes;
		}

		/** A response's body, no more than the range asked for, closing the response with it. */
		class ResponseStream : Stream
		{
			readonly HttpWebResponse response; readonly Stream inner; long left;
			public ResponseStream(HttpWebResponse r, long length) { response = r; inner = r.GetResponseStream(); left = length; }
			public override int Read(byte[] buffer, int offset, int count)
			{
				if (left <= 0) return 0;
				int read = inner.Read(buffer, offset, (int)Math.Min(count, left));
				left -= read;
				return read;
			}
			protected override void Dispose(bool disposing) { if (disposing) { inner.Dispose(); response.Dispose(); } base.Dispose(disposing); }
			public override bool CanRead { get { return true; } }
			public override bool CanSeek { get { return false; } }
			public override bool CanWrite { get { return false; } }
			public override long Length { get { throw new NotSupportedException(); } }
			public override long Position { get { throw new NotSupportedException(); } set { throw new NotSupportedException(); } }
			public override void Flush() { }
			public override long Seek(long o, SeekOrigin s) { throw new NotSupportedException(); }
			public override void SetLength(long v) { throw new NotSupportedException(); }
			public override void Write(byte[] b, int o, int c) { throw new NotSupportedException(); }
		}

		/** A stretch of the whole Setup over the parts it crosses, each part's range asked for when the reading reaches it. */
		class Spanning : Stream
		{
			readonly SetupSource source; long at, left; Stream current; long currentLeft;
			public Spanning(SetupSource s, long from, long length) { source = s; at = from; left = length; }
			public override int Read(byte[] buffer, int offset, int count)
			{
				if (left <= 0) return 0;
				if (current == null || currentLeft <= 0)
				{
					if (current != null) current.Dispose();
					SetupParts.Part part = source.Parts.Parts.First(p => at >= p.At && at < p.At + p.Size);
					long inPart = at - part.At, length = Math.Min(left, part.Size - inPart);
					current = source.OpenOne(source.Base + "/" + part.Name, inPart, length, part.Size);
					currentLeft = length;
				}
				int read = current.Read(buffer, offset, (int)Math.Min(count, currentLeft));
				if (read <= 0) throw new IOException("a part of the Setup stopped early");
				currentLeft -= read; left -= read; at += read;
				return read;
			}
			protected override void Dispose(bool disposing) { if (disposing && current != null) current.Dispose(); base.Dispose(disposing); }
			public override bool CanRead { get { return true; } }
			public override bool CanSeek { get { return false; } }
			public override bool CanWrite { get { return false; } }
			public override long Length { get { throw new NotSupportedException(); } }
			public override long Position { get { throw new NotSupportedException(); } set { throw new NotSupportedException(); } }
			public override void Flush() { }
			public override long Seek(long o, SeekOrigin s) { throw new NotSupportedException(); }
			public override void SetLength(long v) { throw new NotSupportedException(); }
			public override void Write(byte[] b, int o, int c) { throw new NotSupportedException(); }
		}
	}
}
