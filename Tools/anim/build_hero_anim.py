# Builds the hero's animation assets in the game project (run once after new clips arrive from the lab; Docs/HANDOFF.md
# "Art"):
#   UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="C:/Dev/Wildreach/Tools/anim/build_hero_anim.py" -unattended -nop4 -nosplash
# 1. the motion-matching databases from Tools/anim/hero_databases.json (GASP 5.8's own database settings, exported as
#    text in the lab, its clips swapped for the hero's) and one normalization set over all of them;
# 2. the combat windows (UWRANS_Window notifies) on the action clips from Data/anim_windows.json;
# 3. a cook label so the MetaHuman face rig (plugin content loaded by path) goes into packaged builds.
import json, os, unreal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
TOOLS = unreal.WRHeroAnimTools
MM = '/Game/Wildreach/Characters/Hero/Anims/MM'
ACTIONS = '/Game/Wildreach/Characters/Hero/Anims/Actions'


def make(path, cls):
    if EAL.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if isinstance(asset, cls):
            return asset
        EAL.delete_asset(path)
    folder, name = path.rsplit('/', 1)
    return AT.create_asset(name, folder, cls, None)


# 1. databases
spec = json.load(open(os.path.join(ROOT, 'Tools', 'anim', 'hero_databases.json')))
dbs = []
for name, lines in spec['databases'].items():
    db = make(MM + '/' + name, unreal.PoseSearchDatabase)
    bad = TOOLS.import_properties(db, lines, True)
    if bad:
        unreal.log_error('hero anim: %s could not use %s' % (name, list(bad)))
    dbs.append(db)
norm = make(spec['normalizationSet'], unreal.PoseSearchNormalizationSet)
norm.set_editor_property('databases', dbs)
EAL.save_loaded_asset(norm, False)
for db in dbs:
    db.set_editor_property('normalization_set', norm)
    EAL.save_loaded_asset(db, False)
unreal.log('hero anim: %d databases' % len(dbs))

# 2. windows
windows = json.load(open(os.path.join(ROOT, 'Data', 'anim_windows.json')))
TAGS = {'trace': 'State.TraceActive', 'combo': 'State.ComboWindow', 'cancel': 'State.CancelWindow', 'iframes': 'State.IFrames'}
placed = 0
for clip, row in windows.items():
    if clip.startswith('_'):
        continue
    seq = unreal.load_asset(ACTIONS + '/' + clip)
    if not seq:
        unreal.log_error('hero anim: no clip ' + clip)
        continue
    TOOLS.clear_windows(seq)
    for key, tag in TAGS.items():
        if key in row and TOOLS.add_window(seq, tag, row[key][0], row[key][1]):
            placed += 1
    EAL.save_loaded_asset(seq, False)
unreal.log('hero anim: %d windows placed' % placed)

# 3. cook label for what's loaded only by path from Data/player_art.json outside /Game
label = make('/Game/Wildreach/Characters/Hero/PAL_HeroRuntime', unreal.PrimaryAssetLabel)
label.set_editor_property('explicit_assets', [unreal.load_asset('/MetaHumanCharacter/Face/ABP_Face')])
rules = label.get_editor_property('rules')
rules.set_editor_property('cook_rule', unreal.PrimaryAssetCookRule.ALWAYS_COOK)
label.set_editor_property('rules', rules)
EAL.save_loaded_asset(label, False)
unreal.log('hero anim: done')
