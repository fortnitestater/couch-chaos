# Couch Chaos 🛋️💥

A free family party game: the **TV is the screen** and every **phone or PC is a controller**.

## Games (36) — ❄️ Winter Edition
Most games only need taps on the phone. Motion games also work with on-screen sticks or a PC keyboard.

| Row | Games |
|-----|-------|
| 🎾 Motion Sports | Smash Tennis · Strike Bowling · Home Run Derby (3D, swing your phone like a racket/arm/bat) |
| ❄️ Winter | Snowball Fight · Ski Slalom · Ice Hockey · Snow Curling · Iceberg Sumo · Frozen Lake · Sled Dash |
| 🃏 Cards & Table | Wild Eights · Bingo · Dice Duel · Four in a Row · Memory Match · Word Rescue · Higher or Lower |
| 💣 Bombs & Party | Bomb Squad · Hot Potato Bomb · Odd Elf Out · Most Likely To · Doodle Dash · Lowest Unique · RPS Royale |
| 🧠 Brain | Trivia Blitz · Color Clash · Count It! · Copy Cat · Quick Math |
| ⚡ Action | Sky Brawl · Light Trails · Paint Wars · Coin Rush · Target Blast · Reflex Rush · Stop the Meter |

**🎉 Party Mode** plays 6 random games (one from each row) and crowns a party champion. **🎲 Surprise Me** picks a game that fits your player count.
Every game starts with a how-to-play screen; everyone taps READY on their phone.

## How to play
1. Open the site on the TV (smart-TV browser, or a laptop/PC plugged into the TV) → **This is the TV** → **Start the party**. Press **F** for fullscreen.
2. Everyone scans the QR code, or opens `…/play.html` and types the 4-letter room code.
3. The 👑 VIP (first to join) gets a remote on their phone (◀ ▶ ▲ ▼ + OK) to browse the TV menu — or use the TV keyboard/mouse. **Esc** = back to the menu.
4. Returning players just open the link again — they rejoin automatically.

**📷 AI camera (no phones needed):** in the lobby, tap *Use this phone as the AI camera* and stand that phone by the TV facing the room. It tracks up to 4 people with body-pose AI (MediaPipe, runs in the browser): swing an arm to hit, point to aim, lean to steer, hand up = button, wave = shake. Camera players join the motion games; the camera phone gets a small TV remote.

Players on mobile data or another network? Add a free relay link in `turn-config.js` (instructions inside).

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
`tv.html/tv.js/tv.css` TV console · `play.html/play.js` controller · `engine.js` shared engine (ArenaGame, QuizGame, particles, characters) · `games-*.js` the games · `catalog.js` the game list · `sfx.js` synth sounds.
Adding a game = write a class with `start/update/draw` (+ optional `onButton/onAct/onPump/onJoin`), or extend `ArenaGame` / `QuizGame`, then add it to `catalog.js`.
