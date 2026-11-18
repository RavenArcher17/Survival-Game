--- DESKTOP OVERRIDES (this lane runs on the DESKTOP PC; these win over any path in the preamble or brief) ---
- Repo: E:\Dev\Wildreach (NOT C:\Dev\Wildreach). The six Fab packs are present in E:\Dev\Wildreach\Content.
- Engine: UE 5.8.3 at D:\UnrealEngine\UE_5.8 (NOT C:\Program Files\Epic Games\UE_5.8). Compile gate:
  & "D:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat" WildreachEditor Win64 Development -Project="E:\Dev\Wildreach\Wildreach.uproject" -WaitMutex
  For commandlets/python use D:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe. Repo Tools\*.ps1 scripts hardcode the C: engine path: pass the D: path or skip them (tests are not run anyway). check_setup's engine check fails on this PC by design; the pack checks pass.
- Design docs: C:\Users\acars\showcase-notes (git pull it first; same path on this PC). C:\Users\acars\.claude\CLAUDE.md does not exist here: skip.
- THE BAR file "C:\Users\acars\wildreach-night\preamble.md" = the preamble above (wild-desktop/build-preamble.md); there is no wildreach-night folder on this PC.
- LANDED line: append to \\192.168.0.150\TrailsRepo\Saved\Handoff\wild-desktop\LANDED.md (create if missing). Later-cook lines: append to \\192.168.0.150\TrailsRepo\Saved\Handoff\wild-desktop\LATER-COOKS.md. Do NOT write DESKTOP-SAYS.md (the coordinator does).
- One heavy UE job at a time on this PC; the coordinator runs only this lane. Never run two Build.bat/UnrealEditor processes at once.
- Keep scratch files inside E:\Dev\Wildreach\.lane-tmp\ (add nothing from it to git).
- Never create a GitHub release or tag. Never force-push, stash, or `git checkout -- .`. Add files by name.
- Final output: `LANDED <lane> <shas>`, plain summary, every requirement-ledger row with done + where, what is NOT finished, and the size of any new binary files.
- Do ALL work yourself in this one process: never spawn sub-agents/background agents, never schedule wakeups or sleep waiting (desktop rule: one lane, no fan-out). Run compiles in the foreground.

EMAILS (Aaron 11-16): any email a lane sends is NEVER signed as Aaron or written in his voice: no "Hi <name>", no "Thanks", no "Aaron, Freedom35 Studios", no name at all. Subject + plain neutral body. This overrides any brief that says "Signed Aaron".

NO EMAIL SENDS (Aaron 11-16: "emails are supposed to be shown to me before being sent. Every single time"): this lane NEVER sends any email, whatever the brief says. Write the draft (subject, recipient role, plain neutral unsigned body) into your final report and stop there; the coordinator shows it to Aaron.

RESEARCH FIRST (Aaron 11-18, via laptop 22:57: "I always want research and all the planning steps before any building is attempted."): ORDER IS ENFORCED. Your FIRST commit is research/<lane>.md (sources, findings, the full plan, the requirement ledger), pushed before ANY code commit. Your LANDED line names the research commit first. If this worktree already has code commits from an earlier run without that doc, write and commit the research + plan doc now, before any more code.
