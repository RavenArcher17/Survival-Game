# Writes the audio section of Docs/CREDITS.md from the fetch report (pages, licences checked on each page), each page's
# own "Author:" line, and the build manifest (which Data/Audio files come from which source).
import json, re, sys, html, urllib.request, collections
# Usage: python -I credits.py <work>  (prints the section; paste it into Docs/CREDITS.md)
root = sys.argv[1]
rep = json.load(open(root + "/dl2/fetch_report.json"))
man = json.load(open(root + "/audio_manifest.json"))
by_src = collections.defaultdict(list)
for f, v in man.items():
    for s in v["sources"]:
        by_src[s].append(f)

kenney = {
    "kenney_rpg": ("RPG Audio", "https://kenney.nl/assets/rpg-audio"),
    "kenney_impact": ("Impact Sounds", "https://kenney.nl/assets/impact-sounds"),
    "kenney_interface": ("Interface Sounds", "https://kenney.nl/assets/interface-sounds"),
    "kenney_ui": ("UI Audio", "https://kenney.nl/assets/ui-audio"),
    "kenney_jingles": ("Music Jingles", "https://kenney.nl/assets/music-jingles"),
}
alias = {"w_crossbow": "w_crossbowdry"}  # both files sit on the same page
UA = {"User-Agent": "Mozilla/5.0 (Wildreach audio credits)"}
pages = {}

def author(page):
    if page in pages: return pages[page]
    if "bigsoundbank" in page:
        pages[page] = "Joseph Sardin (BigSoundBank.com)"
        return pages[page]
    t = urllib.request.urlopen(urllib.request.Request(page, headers=UA), timeout=60).read().decode("utf-8", "replace")
    m = re.search(r"field-name-author-submitter.{0,700}", t, re.S)
    flat = re.sub(r"\s+", " ", html.unescape(re.sub("<[^>]+>", " ", m.group(0)))) if m else ""
    am = re.search(r"Author:\s*(.*?)(?:\s+(?:Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday),|$)", flat)
    a = am.group(1).strip() if am else "?"
    a = re.sub(r"\s*(Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday),.*$", "", a)
    a = a.replace("( ", "(").replace(" )", ")").strip()
    a = re.sub(r"^\((.*?)\)", r"\1", a).replace("Submitted by", "submitted by")
    pages[page] = a
    return a

def short(files):
    groups = collections.defaultdict(int)
    for f in files:
        groups[re.sub(r"_\d+$", "", f)] += 1
    return ", ".join(f"{g}" + (f" (x{n})" if n > 1 else "") for g, n in sorted(groups.items()))

rows = []
merged = collections.OrderedDict()
for src in sorted(by_src):
    page = kenney[src][1] if src in kenney else rep[alias.get(src, src)]["page"]
    merged.setdefault(page, []).append(src)
by_page_files = {pg: [f for s_ in srcs for f in by_src[s_]] for pg, srcs in merged.items()}
for page, srcs in merged.items():
    src = srcs[0]
    by_src[src] = by_page_files[page]
    files = by_src[src]
    if src in kenney:
        title, page = kenney[src]
        rows.append((title, "Kenney (kenney.nl)", "CC0 1.0", page, "", files))
        continue
    r = rep[alias.get(src, src)]
    lic = r.get("licence") or []
    pick = "CC0 1.0" if "CC0" in lic else ("CC-BY 3.0" if "CC-BY 3.0" in lic else ", ".join(lic))
    title = re.sub(r"\s*[^\w\s]*\s*Free (Sound Effect )?Download.*$", "", r.get("title", "?")).replace(" | OpenGameArt.org", "").strip()
    rows.append((title, author(r["page"]), pick, r["page"], r["url"], files))

out = []
out.append("## Audio (the sound pass)")
out.append("")
out.append("Every sound and music file in `Data/Audio` comes from a free library whose licence allows commercial use and")
out.append("redistribution, checked on each source's own page when it was downloaded (2026-10-26). Nearly all of it is CC0")
out.append("(public domain: no attribution needed, credited here anyway). Two sources are **CC-BY 3.0** and must stay credited")
out.append("wherever the game is distributed: *Lava loop* (Blender Foundation) and *Crossbow sounds* (HolgiB). Where a page offered")
out.append("several licences (CC0 alongside CC-BY / GPL / OGA-BY) the CC0 option is the one used.")
out.append("The files were cut, mixed to mono for 3D effects, loudness-matched and re-encoded as Ogg Vorbis by")
out.append("`Tools/audio/build_audio.py`; `Data/Audio/sounds.json` says where each one plays.")
out.append("")
out.append("| Source | Author | Licence | Page | Used for (Data/Audio/...) |")
out.append("| --- | --- | --- | --- | --- |")
for title, a, lic, page, url, files in rows:
    out.append(f"| {title} | {a} | {lic} | {page} | {short(files)} |")
out.append("")
print("\n".join(out))
cc_by = [r for r in rows if "CC-BY" in r[2]]
print("\nCC-BY rows:", [(r[0], r[1]) for r in cc_by], file=sys.stderr)
