# Checks each source page (licence, the file is linked there) and downloads the file into its own folder under
# <work>/dl2/<key>; Kenney's packs go to <work>/dl/kenney_<pack>. Unzip / un-7z them there (py7zr), then run
# build_audio.py <work> Data/Audio and make_table.py. Never run anything from the downloads.
# Usage: python -I fetch_sources.py <work> [check]
import json, os, re, sys, urllib.parse, urllib.request, html

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sources import ITEMS, KENNEY

WORK = sys.argv[1]
DL = os.path.join(WORK, "dl2")
CHECK_ONLY = len(sys.argv) > 2
UA = {"User-Agent": "Mozilla/5.0 (Wildreach audio pass)"}

def get(url):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.status, r.read()

report = {}
pages = {}
for key, (page, url) in ITEMS.items():
    info = {"page": page, "url": url}
    try:
        if page not in pages:
            pages[page] = get(page)[1].decode("utf-8", "replace")
        text = pages[page]
        if "opengameart.org" in page:
            m = re.search(r'field-name-field-art-licenses.{0,2000}?</div>\s*</div>\s*</div>', text, re.S)
            block = m.group(0) if m else ""
            info["licence"] = sorted(set(re.findall(r'>\s*((?:CC0|CC-BY[^<]*|OGA-BY[^<]*|GPL[^<]*|LGPL[^<]*))\s*<', block)))
            a = re.search(r'field-name-author-submitter.{0,600}?href="/users/([^"]+)"[^>]*>([^<]+)<', text, re.S)
            info["submitter"] = html.unescape(a.group(2)) if a else "?"
            ac = re.search(r'Author:\s*(?:<[^>]+>)*\s*([^<]{2,80})', text)
            title = re.search(r'<title>([^<|]+)', text)
            info["title"] = html.unescape(title.group(1).strip()) if title else "?"
            fname = url.rsplit("/", 1)[1]
            info["linked"] = fname in text or urllib.parse.unquote(fname) in text
        else:
            info["licence"] = ["CC0"] if re.search(r'CC0', text) else []
            title = re.search(r'<title>([^<]+)', text)
            info["title"] = html.unescape(title.group(1).strip()) if title else "?"
            info["submitter"] = "Joseph Sardin (BigSoundBank)"
            info["linked"] = True
        if not CHECK_ONLY and info["licence"]:
            folder = os.path.join(DL, key)
            os.makedirs(folder, exist_ok=True)
            out = os.path.join(folder, urllib.parse.unquote(url.rsplit("/", 1)[1]))
            if not os.path.exists(out):
                status, data = get(url)
                with open(out, "wb") as f:
                    f.write(data)
            info["file"] = out
            info["bytes"] = os.path.getsize(out)
    except Exception as e:
        info["error"] = str(e)
    report[key] = info
    print(key, info.get("licence"), info.get("submitter"), info.get("linked"), info.get("bytes", ""), info.get("error", ""), flush=True)

for pack in KENNEY:
    page = "https://kenney.nl/assets/" + pack
    text = get(page)[1].decode("utf-8", "replace")
    zips = re.findall(r"https://kenney.nl/media/pages/assets/" + pack + r"/[^\"']*\.zip", text)
    ok = "creativecommons.org/publicdomain/zero" in text
    print("kenney", pack, "CC0" if ok else "LICENCE?", zips[:1], flush=True)
    if zips and ok and not CHECK_ONLY:
        folder = os.path.join(WORK, "dl", "kenney_" + pack)
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, "pack.zip"), "wb") as out:
            out.write(get(zips[0])[1])

os.makedirs(DL, exist_ok=True)
with open(os.path.join(DL, "fetch_report.json"), "w") as out:
    json.dump(report, out, indent=1)
