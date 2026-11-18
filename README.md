# Couch Chaos 🛋️💥

A free family party game: the **TV is the screen** and every **phone or PC is a controller**.

## Games
| # | Game | Controls |
|---|------|----------|
| 1 | ✈️ **Sky Brawl** – fighter-plane dogfight, power-ups (triple shot, homing missiles, shield, mega plane, turbo) and random chaos events (meteor shower, reverse controls, bullet frenzy, tiny planes) | Tilt phone like a steering wheel / drag / ← → · FIRE · BOOST |
| 2 | 🫧 **Sumo Blobs** – push everyone into the lava, shrinking arena, ice round, mega-bounce round | Tilt / drag / WASD · HEAVY · DASH |
| 3 | 🃏 **Wild Eights** – crazy-eights card game, your hand is secret on your phone. 8 wild, 2 = +2 (stackable), J skip, Q reverse, **A = CHAOS: every hand passes to the next player** | Tap cards · PC: ← → Enter, D draw, P pass |
| 4 | ⚡ **Reflex Rush** – tap on GO!… fakes like GOAT / GO? / G0! cost points | Big button / Space |
| 5 | 🚀 **Shake Race** – shake the phone to fly your rocket, lightning zaps the leader | Shake / tap / alternate ← → |

Trophies (3/2/1) build up across games in the lobby. If you're alone, bots join so you can practise.

## How to play
1. Open the site on the TV (smart-TV browser, or a laptop/PC plugged into the TV) → **This is the TV** → **Start the party**. Press **F** for fullscreen.
2. Everyone scans the QR code, or opens `…/play.html` and types the 4-letter room code.
3. The 👑 VIP (first to join) picks the game on their controller (or press 1–5 on the TV keyboard). **Esc** on the TV = back to lobby.

Phones: motion needs **HTTPS** (every host below gives you that). On iPhone, tap "Allow" for motion when joining.
If tilt steers the wrong way, tap **⇄** on the controller; **⟲ Center** sets "straight".

## Host it for free (pick one)

It's a plain static site: no server or database needed. Players connect directly to the TV with WebRTC through the free PeerJS service.

**Option A: GitHub Pages → `yourname.github.io/couch-chaos`**
1. Make a free account at github.com → **New repository** → name it `couch-chaos`, public.
2. **Add file → Upload files** → drag in everything from this folder → Commit.
3. **Settings → Pages** → Source: *Deploy from a branch*, Branch: `main` / root → Save.
4. After ~1 minute the site is live at `https://yourname.github.io/couch-chaos/`.

**Option B: Netlify → `something.netlify.app`**
1. Free account at netlify.com → **Add new site → Deploy manually**.
2. Drag this whole folder onto the page. Done; rename the site under *Site configuration*.

**Option C: Cloudflare Pages → `something.pages.dev`**: free account → Workers & Pages → Create → Pages → *Upload assets* → drag the folder.

## Run it locally (for testing)
```
python -m http.server 8000
```
then open http://localhost:8000. Phones can't use tilt over plain http, so use the hosted version for that.

## Files
`tv.html/tv.js` TV host · `play.html/play.js` controller · `games.js` all games · `sfx.js` synth sounds · `style.css` · `shared.js`.
Adding a game = write a class with `start/update/draw` (+ optional `onButton/onAct/onPump/onJoin`) in `games.js` and add it to `GAMES`.
