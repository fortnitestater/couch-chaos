/* Couch Chaos — 🎵 SONG GUESS. The TV plays a tiny bit of a famous song (0.1 s!), then more and more.
   Everyone types on their phone (live suggestions) — earlier guesses score more.
   Audio = official 30-second iTunes previews, decoded with Web Audio so even 0.1 s clips are exact. */
const SONGS = [
  // [title, artist, year]
  ['Shake It Off', 'Taylor Swift', 2014], ['Blank Space', 'Taylor Swift', 2014], ['Love Story', 'Taylor Swift', 2008], ['Anti-Hero', 'Taylor Swift', 2022],
  ['Cruel Summer', 'Taylor Swift', 2019], ['You Belong With Me', 'Taylor Swift', 2008], ['We Are Never Ever Getting Back Together', 'Taylor Swift', 2012],
  ['Bad Blood', 'Taylor Swift', 2014], ['Style', 'Taylor Swift', 2014], ['Look What You Made Me Do', 'Taylor Swift', 2017], ['cardigan', 'Taylor Swift', 2020],
  ['Wake Me Up', 'Avicii', 2013], ['Levels', 'Avicii', 2011], ['Hey Brother', 'Avicii', 2013], ['The Nights', 'Avicii', 2014], ['Waiting For Love', 'Avicii', 2015],
  ['Addicted To You', 'Avicii', 2013], ['SOS', 'Avicii', 2019],
  ['Shape of You', 'Ed Sheeran', 2017], ['Perfect', 'Ed Sheeran', 2017], ['Thinking Out Loud', 'Ed Sheeran', 2014], ['Bad Habits', 'Ed Sheeran', 2021], ['Photograph', 'Ed Sheeran', 2014],
  ['Blinding Lights', 'The Weeknd', 2019], ['Starboy', 'The Weeknd', 2016], ['Save Your Tears', 'The Weeknd', 2020], ["Can't Feel My Face", 'The Weeknd', 2015],
  ['Levitating', 'Dua Lipa', 2020], ["Don't Start Now", 'Dua Lipa', 2019], ['New Rules', 'Dua Lipa', 2017], ['Houdini', 'Dua Lipa', 2023],
  ['As It Was', 'Harry Styles', 2022], ['Watermelon Sugar', 'Harry Styles', 2019],
  ['bad guy', 'Billie Eilish', 2019], ['BIRDS OF A FEATHER', 'Billie Eilish', 2024], ['lovely', 'Billie Eilish', 2018],
  ['Uptown Funk', 'Mark Ronson', 2014], ['Just the Way You Are', 'Bruno Mars', 2010], ['24K Magic', 'Bruno Mars', 2016], ['Locked Out of Heaven', 'Bruno Mars', 2012],
  ["That's What I Like", 'Bruno Mars', 2016], ['Die With A Smile', 'Lady Gaga', 2024], ['Grenade', 'Bruno Mars', 2010],
  ['Rolling in the Deep', 'Adele', 2010], ['Someone Like You', 'Adele', 2011], ['Hello', 'Adele', 2015], ['Easy On Me', 'Adele', 2021], ['Skyfall', 'Adele', 2012],
  ['Roar', 'Katy Perry', 2013], ['Firework', 'Katy Perry', 2010], ['Dark Horse', 'Katy Perry', 2013], ['Teenage Dream', 'Katy Perry', 2010], ['Last Friday Night (T.G.I.F.)', 'Katy Perry', 2011],
  ['Bad Romance', 'Lady Gaga', 2009], ['Telephone', 'Lady Gaga', 2009], ['Shallow', 'Lady Gaga', 2018], ['Just Dance', 'Lady Gaga', 2008],
  ['Umbrella', 'Rihanna', 2007], ['Diamonds', 'Rihanna', 2012], ['We Found Love', 'Rihanna', 2011], ['Only Girl (In the World)', 'Rihanna', 2010],
  ['Single Ladies (Put a Ring on It)', 'Beyoncé', 2008], ['Crazy In Love', 'Beyoncé', 2003], ['Halo', 'Beyoncé', 2008],
  ['Baby', 'Justin Bieber', 2010], ['Sorry', 'Justin Bieber', 2015], ['Love Yourself', 'Justin Bieber', 2015], ['Peaches', 'Justin Bieber', 2021], ['STAY', 'The Kid LAROI', 2021],
  ['Viva la Vida', 'Coldplay', 2008], ['Fix You', 'Coldplay', 2005], ['Yellow', 'Coldplay', 2000], ['Paradise', 'Coldplay', 2011], ['A Sky Full of Stars', 'Coldplay', 2014],
  ['Something Just Like This', 'The Chainsmokers', 2017], ['Closer', 'The Chainsmokers', 2016],
  ['Believer', 'Imagine Dragons', 2017], ['Radioactive', 'Imagine Dragons', 2012], ['Thunder', 'Imagine Dragons', 2017], ['Demons', 'Imagine Dragons', 2012], ['Enemy', 'Imagine Dragons', 2021],
  ['Bohemian Rhapsody', 'Queen', 1975], ["Don't Stop Me Now", 'Queen', 1978], ['We Will Rock You', 'Queen', 1977], ['Another One Bites the Dust', 'Queen', 1980],
  ['Billie Jean', 'Michael Jackson', 1982], ['Beat It', 'Michael Jackson', 1982], ['Thriller', 'Michael Jackson', 1982], ['Smooth Criminal', 'Michael Jackson', 1987], ['Man in the Mirror', 'Michael Jackson', 1987],
  ['Dancing Queen', 'ABBA', 1976], ['Mamma Mia', 'ABBA', 1975], ['Gimme! Gimme! Gimme! (A Man After Midnight)', 'ABBA', 1979],
  ['Africa', 'TOTO', 1982], ['Take On Me', 'a-ha', 1985], ["Don't Stop Believin'", 'Journey', 1981], ["Livin' On A Prayer", 'Bon Jovi', 1986], ['Eye of the Tiger', 'Survivor', 1982],
  ['I Wanna Dance with Somebody (Who Loves Me)', 'Whitney Houston', 1987], ['I Will Always Love You', 'Whitney Houston', 1992], ['Girls Just Want to Have Fun', 'Cyndi Lauper', 1983],
  ['Last Christmas', 'Wham!', 1984], ['All I Want for Christmas Is You', 'Mariah Carey', 1994], ['Never Gonna Give You Up', 'Rick Astley', 1987], ['The Final Countdown', 'Europe', 1986],
  ['Smells Like Teen Spirit', 'Nirvana', 1991], ['Wonderwall', 'Oasis', 1995], ['I Want It That Way', 'Backstreet Boys', 1999], ['...Baby One More Time', 'Britney Spears', 1998],
  ['Toxic', 'Britney Spears', 2003], ['Wannabe', 'Spice Girls', 1996], ['Macarena', 'Los Del Rio', 1995], ['Lose Yourself', 'Eminem', 2002], ['Without Me', 'Eminem', 2002],
  ['Hey Ya!', 'Outkast', 2003], ['Mr. Brightside', 'The Killers', 2003], ["Hips Don't Lie", 'Shakira', 2006], ['Waka Waka (This Time for Africa)', 'Shakira', 2010],
  ['I Gotta Feeling', 'Black Eyed Peas', 2009], ['Happy', 'Pharrell Williams', 2013], ['Gangnam Style', 'PSY', 2012], ['Despacito', 'Luis Fonsi', 2017],
  ['Party Rock Anthem', 'LMFAO', 2011], ['Somebody That I Used to Know', 'Gotye', 2011], ['Call Me Maybe', 'Carly Rae Jepsen', 2011], ['Get Lucky', 'Daft Punk', 2013],
  ['What Makes You Beautiful', 'One Direction', 2011], ['Story of My Life', 'One Direction', 2013], ['Moves Like Jagger', 'Maroon 5', 2011], ['Sugar', 'Maroon 5', 2014], ['Girls Like You', 'Maroon 5', 2018],
  ['Someone You Loved', 'Lewis Capaldi', 2018], ['Circles', 'Post Malone', 2019], ['Sunflower', 'Post Malone', 2018], ['Old Town Road', 'Lil Nas X', 2019],
  ['drivers license', 'Olivia Rodrigo', 2021], ['good 4 u', 'Olivia Rodrigo', 2021], ['Flowers', 'Miley Cyrus', 2023], ['Wrecking Ball', 'Miley Cyrus', 2013], ['Party In The U.S.A.', 'Miley Cyrus', 2009],
  ['Espresso', 'Sabrina Carpenter', 2024], ['Please Please Please', 'Sabrina Carpenter', 2024], ['Heat Waves', 'Glass Animals', 2020], ['Dance Monkey', 'Tones And I', 2019],
  ['Summer', 'Calvin Harris', 2014], ['This Is What You Came For', 'Calvin Harris', 2016], ['Titanium', 'David Guetta', 2011], ["Don't You Worry Child", 'Swedish House Mafia', 2012],
  ['Lean On', 'Major Lazer', 2015], ['Rather Be', 'Clean Bandit', 2014], ['Chandelier', 'Sia', 2014], ['Cheap Thrills', 'Sia', 2016], ['Give Me Everything', 'Pitbull', 2011],
  ['I Will Survive', 'Gloria Gaynor', 1978], ["Stayin' Alive", 'Bee Gees', 1977], ['Y.M.C.A.', 'Village People', 1978], ['September', 'Earth, Wind & Fire', 1978],
  ['Hey Jude', 'The Beatles', 1968], ['Here Comes The Sun', 'The Beatles', 1969], ["Can't Help Falling In Love", 'Elvis Presley', 1961], ['Stand By Me', 'Ben E. King', 1961],
  ['Lean On Me', 'Bill Withers', 1972], ["Sweet Child O' Mine", "Guns N' Roses", 1987], ['Highway to Hell', 'AC/DC', 1979], ['In the End', 'Linkin Park', 2000], ['Numb', 'Linkin Park', 2003],
  ['Complicated', 'Avril Lavigne', 2002], ['Since U Been Gone', 'Kelly Clarkson', 2004], ['7 Years', 'Lukas Graham', 2015],
  ['Counting Stars', 'OneRepublic', 2013], ['Say So', 'Doja Cat', 2019], ['Dynamite', 'BTS', 2020], ['7 rings', 'Ariana Grande', 2019], ['thank u, next', 'Ariana Grande', 2018],
  ['Señorita', 'Shawn Mendes', 2019], ['Havana', 'Camila Cabello', 2017], ['See You Again', 'Wiz Khalifa', 2015], ['Faded', 'Alan Walker', 2015], ['Stay With Me', 'Sam Smith', 2014],
  ['All of Me', 'John Legend', 2013], ['Let Her Go', 'Passenger', 2012], ['Take Me To Church', 'Hozier', 2013], ['Riptide', 'Vance Joy', 2013], ['Pumped Up Kicks', 'Foster The People', 2010],
  ['We Are Young', 'fun.', 2011], ['Do I Wanna Know?', 'Arctic Monkeys', 2013], ['Running Up That Hill (A Deal With God)', 'Kate Bush', 1985], ['Lose Control', 'Teddy Swims', 2023],
  ['Beautiful Things', 'Benson Boone', 2024], ['Good Luck, Babe!', 'Chappell Roan', 2024], ['Seven Nation Army', 'The White Stripes', 2003], ['Mambo No. 5', 'Lou Bega', 1999],
  ['Bailando', 'Enrique Iglesias', 2014], ['Timber', 'Pitbull', 2013], ['Rasputin', 'Boney M.', 1978], ['Barbie Girl', 'Aqua', 1997], ['Blue (Da Ba Dee)', 'Eiffel 65', 1998],
];
const SONG_IDS = {"Shake It Off|Taylor Swift":1440933651,"Blank Space|Taylor Swift":1440933517,"Love Story|Taylor Swift":1440924808,"Anti-Hero|Taylor Swift":1649434293,"Cruel Summer|Taylor Swift":1468058171,"You Belong With Me|Taylor Swift":1440925035,"We Are Never Ever Getting Back Together|Taylor Swift":1440935564,"Bad Blood|Taylor Swift":1440936028,"Style|Taylor Swift":1440933520,"Look What You Made Me Do|Taylor Swift":1440934255,"cardigan|Taylor Swift":1524801580,"Wake Me Up|Avicii":1440872929,"Levels|Avicii":1440787349,"Hey Brother|Avicii":1440873108,"The Nights|Avicii":1440829549,"Waiting For Love|Avicii":1440834312,"Addicted To You|Avicii":1440873120,"SOS|Avicii":1462628897,"Shape of You|Ed Sheeran":1193701392,"Perfect|Ed Sheeran":1193701400,"Thinking Out Loud|Ed Sheeran":1050204631,"Bad Habits|Ed Sheeran":1571330212,"Photograph|Ed Sheeran":1050204626,"Blinding Lights|The Weeknd":1499378607,"Starboy|The Weeknd":1440870375,"Save Your Tears|The Weeknd":1505683980,"Can't Feel My Face|The Weeknd":1440858500,"Levitating|Dua Lipa":1538003843,"Don't Start Now|Dua Lipa":1484636829,"New Rules|Dua Lipa":1228742334,"Houdini|Dua Lipa":1714502827,"As It Was|Harry Styles":1615585008,"Watermelon Sugar|Harry Styles":1485802967,"bad guy|Billie Eilish":1450695739,"BIRDS OF A FEATHER|Billie Eilish":1739659142,"lovely|Billie Eilish":1369380479,"Uptown Funk|Mark Ronson":943946671,"Just the Way You Are|Bruno Mars":576655095,"24K Magic|Bruno Mars":1161504024,"Locked Out of Heaven|Bruno Mars":573962551,"That's What I Like|Bruno Mars":1161504043,"Die With A Smile|Lady Gaga":1762656732,"Grenade|Bruno Mars":576655094,"Rolling in the Deep|Adele":1544491233,"Someone Like You|Adele":1544491998,"Hello|Adele":1544494392,"Easy On Me|Adele":1590036021,"Skyfall|Adele":566322365,"Roar|Katy Perry":1440818897,"Firework|Katy Perry":716192625,"Dark Horse|Katy Perry":1440819170,"Teenage Dream|Katy Perry":716192601,"Last Friday Night (T.G.I.F.)|Katy Perry":716084619,"Bad Romance|Lady Gaga":1476095084,"Shallow|Lady Gaga":1434371887,"Umbrella|Rihanna":1441154437,"Diamonds|Rihanna":1444321282,"We Found Love|Rihanna":1440758945,"Only Girl (In the World)|Rihanna":6836706212,"Single Ladies (Put a Ring on It)|Beyoncé":296016899,"Crazy In Love|Beyoncé":262218448,"Halo|Beyoncé":296016893,"Baby|Justin Bieber":1440661545,"Sorry|Justin Bieber":1440829610,"Love Yourself|Justin Bieber":1440829613,"Peaches|Justin Bieber":1556170101,"STAY|The Kid LAROI":1574968888,"Viva la Vida|Coldplay":1122773680,"Fix You|Coldplay":1123076826,"Yellow|Coldplay":1122782283,"Paradise|Coldplay":726373148,"A Sky Full of Stars|Coldplay":829910927,"Something Just Like This|The Chainsmokers":1207120448,"Closer|The Chainsmokers":1170699703,"Believer|Imagine Dragons":1411628233,"Radioactive|Imagine Dragons":1440873126,"Thunder|Imagine Dragons":1411629089,"Demons|Imagine Dragons":1440873339,"Enemy|Imagine Dragons":1591640885,"Bohemian Rhapsody|Queen":6781024437,"Don't Stop Me Now|Queen":6781079518,"We Will Rock You|Queen":6781080737,"Another One Bites the Dust|Queen":6781076397,"Billie Jean|Michael Jackson":269573364,"Beat It|Michael Jackson":269573341,"Thriller|Michael Jackson":269573303,"Smooth Criminal|Michael Jackson":159294551,"Man in the Mirror|Michael Jackson":477803872,"Dancing Queen|ABBA":1422648513,"Mamma Mia|ABBA":1440861294,"Gimme! Gimme! Gimme! (A Man After Midnight)|ABBA":1422648970,"Africa|TOTO":185717604,"Take On Me|a-ha":380907765,"Don't Stop Believin'|Journey":160024096,"Livin' On A Prayer|Bon Jovi":1422955211,"Eye of the Tiger|Survivor":254685026,"I Wanna Dance with Somebody (Who Loves Me)|Whitney Houston":840431935,"I Will Always Love You|Whitney Houston":388151901,"Girls Just Want to Have Fun|Cyndi Lauper":400603693,"Last Christmas|Wham!":282658482,"All I Want for Christmas Is You|Mariah Carey":585972803,"Never Gonna Give You Up|Rick Astley":1559885421,"The Final Countdown|Europe":196480329,"Smells Like Teen Spirit|Nirvana":1440783625,"Wonderwall|Oasis":1517447333,"I Want It That Way|Backstreet Boys":283567164,"...Baby One More Time|Britney Spears":273143820,"Toxic|Britney Spears":251948354,"Wannabe|Spice Girls":1573849255,"Macarena|Los Del Rio":1518348098,"Lose Yourself|Eminem":1440903439,"Without Me|Eminem":1440821643,"Hey Ya!|Outkast":1774799818,"Mr. Brightside|The Killers":1440891171,"Hips Don't Lie|Shakira":1817217063,"Waka Waka (This Time for Africa)|Shakira":371784883,"I Gotta Feeling|Black Eyed Peas":1440769310,"Happy|Pharrell Williams":863835363,"Gangnam Style|PSY":1445144527,"Despacito|Luis Fonsi":1447401620,"Party Rock Anthem|LMFAO":1440636627,"Somebody That I Used to Know|Gotye":1440754487,"Call Me Maybe|Carly Rae Jepsen":1440874008,"Get Lucky|Daft Punk":617154366,"What Makes You Beautiful|One Direction":482734113,"Story of My Life|One Direction":695312371,"Moves Like Jagger|Maroon 5":1440859510,"Sugar|Maroon 5":1440855562,"Girls Like You|Maroon 5":1396141911,"Someone You Loved|Lewis Capaldi":1452619054,"Circles|Post Malone":1477880561,"Sunflower|Post Malone":1445949267,"Old Town Road|Lil Nas X":1468166468,"drivers license|Olivia Rodrigo":1560735480,"good 4 u|Olivia Rodrigo":1582277652,"Flowers|Miley Cyrus":1674691586,"Wrecking Ball|Miley Cyrus":1460315319,"Party In The U.S.A.|Miley Cyrus":1445007713,"Espresso|Sabrina Carpenter":1752214923,"Please Please Please|Sabrina Carpenter":1795513492,"Heat Waves|Glass Animals":1508562516,"Summer|Calvin Harris":922876189,"This Is What You Came For|Calvin Harris":1108212668,"Titanium|David Guetta":693226464,"Lean On|Major Lazer":1696639397,"Rather Be|Clean Bandit":1027563268,"Chandelier|Sia":882945383,"Cheap Thrills|Sia":1055074640,"Give Me Everything|Pitbull":1712042664,"I Will Survive|Gloria Gaynor":1443818368,"Stayin' Alive|Bee Gees":1440900114,"Y.M.C.A.|Village People":1440825374,"September|Earth, Wind & Fire":1456623340,"Hey Jude|The Beatles":1441133277,"Here Comes The Sun|The Beatles":1441164589,"Can't Help Falling In Love|Elvis Presley":388128266,"Stand By Me|Ben E. King":738196409,"Lean On Me|Bill Withers":391725484,"Sweet Child O' Mine|Guns N' Roses":1377813701,"Highway to Hell|AC/DC":574044008,"In the End|Linkin Park":590431785,"Numb|Linkin Park":590423552,"Complicated|Avril Lavigne":315025823,"Since U Been Gone|Kelly Clarkson":275765380,"7 Years|Lukas Graham":1081573445,"Counting Stars|OneRepublic":1471704175,"Say So|Doja Cat":1486465101,"Dynamite|BTS":1596532400,"7 rings|Ariana Grande":1450334168,"thank u, next|Ariana Grande":1450334169,"Señorita|Shawn Mendes":1468910018,"Havana|Camila Cabello":1321217032,"See You Again|Wiz Khalifa":966411602,"Faded|Alan Walker":1196294581,"Stay With Me|Sam Smith":1440814425,"All of Me|John Legend":1441844542,"Let Her Go|Passenger":1623014090,"Take Me To Church|Hozier":900672609,"Riptide|Vance Joy":1022164261,"Pumped Up Kicks|Foster The People":435761212,"We Are Young|fun.":486040195,"Do I Wanna Know?|Arctic Monkeys":663097965,"Running Up That Hill (A Deal With God)|Kate Bush":1675375090,"Lose Control|Teddy Swims":1691699836,"Beautiful Things|Benson Boone":1724488124,"Good Luck, Babe!|Chappell Roan":1737497080,"Seven Nation Army|The White Stripes":1533513537,"Just Dance|Lady Gaga":1445006767,"Dance Monkey|Tones And I":1475930045,"Don't You Worry Child|Swedish House Mafia":6798410339,"Mambo No. 5|Lou Bega":1322068804,"Bailando|Enrique Iglesias":1440820189,"Timber|Pitbull":737053283,"Rasputin|Boney M.":553356946,"Barbie Girl|Aqua":1443758761,"Blue (Da Ba Dee)|Eiffel 65":257424857,"Telephone|Lady Gaga":1476727675}; // exact iTunes tracks (checked: real artist, no covers/remixes)
const SONG_STAGES = [0.1, 0.5, 1, 2, 4, 7, 10];
const SONG_PTS = [1000, 800, 650, 500, 380, 260, 150];
const SONG_WAIT = 10; // seconds to guess after each clip
const songNorm = s => String(s).normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/&/g, 'and').replace(/[^a-z0-9 ]/g, ' ').replace(/\s+/g, ' ').trim();
const songLabel = s => `${s[0]} — ${s[1]}`;
const SONG_LIST = SONGS.map(songLabel);

/* finds + decodes the iTunes preview for a song */
async function songPick(song) {
  const [title, artist] = song, nt = songNorm(title), na = songNorm(artist);
  const url = `https://itunes.apple.com/search?media=music&entity=song&limit=25&term=${encodeURIComponent(artist + ' ' + title)}`;
  const j = await fetch(url).then(r => (r.ok ? r.json() : Promise.reject(new Error('search ' + r.status))));
  const badT = /\b(karaoke|instrumental|cover|tribute|remix|remixed|live|acoustic|sped|slowed|mix|8.?bit|lullaby|piano|orchestral|radio edit|demo)\b/i;
  const badC = /\b(karaoke|tribute|lullab|cover|covers|piano|instrumental|remixes|made famous|in the style|workout|kids)\b/i;
  const score = r => {
    if (!r.previewUrl) return -1;
    const rt = songNorm(r.trackName || ''), ra = songNorm(r.artistName || ''); let s = 0;
    if (!(ra.includes(na) || na.includes(ra))) return -1;           // must be the real artist
    if (rt === nt) s += 6; else if (rt.startsWith(nt)) s += 3; else if (rt.includes(nt)) s += 1; else return -1;
    if (badT.test(r.trackName) && !badT.test(title)) s -= 8;
    if (badC.test(r.collectionName || '')) s -= 8;
    return s;
  };
  const best = (j.results || []).map(r => ({ r, s: score(r) })).filter(x => x.s > 0).sort((a, b) => b.s - a.s)[0];
  if (!best) throw new Error('no preview for ' + title);
  return best.r;
}
/* one request for the whole playlist: exact tracks by iTunes id (falls back to a search) */
async function songLookup(songs) {
  const ids = songs.map(s => SONG_IDS[s[0] + '|' + s[1]]).filter(Boolean), map = new Map();
  for (let i = 0; i < ids.length; i += 120) {
    try {
      const j = await fetch(`https://itunes.apple.com/lookup?id=${ids.slice(i, i + 120).join(',')}`).then(r => r.json());
      for (const r of j.results || []) if (r.previewUrl) map.set(r.trackId, r);
    } catch (e) { console.warn('[song] lookup', e); }
  }
  return map;
}
async function songFetch(song, ac, lookup) {
  const id = SONG_IDS[song[0] + '|' + song[1]], known = id && lookup ? (await lookup).get(id) : null;
  const r = known || await songPick(song), best = { r };
  const buf = await fetch(best.r.previewUrl).then(r => r.arrayBuffer()).then(b => ac.decodeAudioData(b));
  // skip the silence at the start so 0.1 s really is music
  const d = buf.getChannelData(0); let pk = 0; for (let i = 0; i < d.length; i += 16) pk = Math.max(pk, Math.abs(d[i]));
  let i0 = 0; while (i0 < d.length && Math.abs(d[i0]) < pk * 0.12) i0 += 16;
  const art = new Image(); art.src = String(best.r.artworkUrl100 || '').replace(/\d+x\d+bb/, '600x600bb');
  return { song, buf, off: Math.max(0, i0 / buf.sampleRate - 0.02), art, album: best.r.collectionName || '' };
}

class SongGuess {
  constructor(c) { this.c = c; this.t = 0; this.fx = new Particles(); this.bars = new Float32Array(48); this.spin = 0; this.queue = []; this.fails = 0; }
  start() {
    this.ps = this.c.roster(1);
    this.score = new Map(this.ps.map(p => [p, 0]));
    this.host = this.ps.find(p => !p.bot && !p.cam) || null;
    this.ac = this.c.sfx.audio && this.c.sfx.audio();
    if (this.ac && this.ac.state === 'suspended') this.ac.resume();
    if (this.ac) { this.out = this.ac.createGain(); this.out.gain.value = 0.9; this.an = this.ac.createAnalyser(); this.an.fftSize = 256; this.an.smoothingTimeConstant = 0.72; this.out.connect(this.an); this.an.connect(this.ac.destination); this.freq = new Uint8Array(this.an.frequencyBinCount); }
    this.phase = 'setup'; this.setupStep = 0; this.total = 10; this.era = 0; this.n = 0;
    if (this.host) this.askSetup(); else this.begin();
  }
  // ---------- setup: the host picks length + era on their phone ----------
  askSetup() {
    const [prompt, opts] = this.setupStep === 0
      ? ['How many songs?', [['5 songs', '#4dabff'], ['10 songs', '#51cf66'], ['20 songs', '#ff922b'], ['♾️ Endless', '#cc5de8']]]
      : ['Which songs?', [['🌍 All the hits', '#51cf66'], ['🔥 2010s → now', '#ff4d6d'], ['📼 Throwbacks (before 2010)', '#ffd43b']]];
    for (const p of this.ps) if (!p.bot) this.c.setLayout(p, p === this.host ? { name: 'choice', prompt, options: opts.map(([label, color]) => ({ label, color })), cols: 1, big: true } : { name: 'msg', emoji: '🎵', text: `${this.host.name} is setting up the game…` });
  }
  begin() {
    const pool = SONGS.filter(s => this.era === 0 || (this.era === 1 ? s[2] >= 2010 : s[2] < 2010));
    this.order = shuffle(pool.slice()); this.oi = 0; this.lookup = songLookup(this.order);
    this.phase = 'load'; this.loadT = 0;
    this.prefetch(); this.prefetch();
    this.c.banner('🎵 SONG GUESS', 1600, '#cc5de8');
  }
  prefetch() {
    if (!this.ac || this.queue.length >= 2 || this.oi >= this.order.length) return;
    const song = this.order[this.oi++], slot = { song, ready: null, fail: false };
    this.queue.push(slot);
    songFetch(song, this.ac, this.lookup).then(r => { slot.ready = r; this.fails = 0; }, e => { console.warn('[song]', e.message); slot.fail = true; this.fails++; });
  }
  // ---------- one song ----------
  nextSong() {
    this.n++;
    this.cur = this.queue.shift().ready; this.prefetch();
    this.idx = SONGS.indexOf(this.cur.song);
    this.got = new Map(); // p -> {stage, pts}
    this.stage = -1; this.replays = 0;
    this.nextStage();
  }
  nextStage() {
    this.stage++;
    if (this.stage >= SONG_STAGES.length) return this.reveal();
    this.st = new Map(); // p -> 'guess' | 'wrong' | 'skip'
    this.phase = 'listen'; this.stageT = 0;
    this.play(SONG_STAGES[this.stage]);
    this.clipEnd = SONG_STAGES[this.stage] + 0.15;
    for (const p of this.ps) if (p.bot && !this.got.has(p)) this.botPlan(p);
    this.pushAll();
  }
  botPlan(p) {
    const know = [0.06, 0.12, 0.2, 0.3, 0.42, 0.52, 0.62][this.stage];
    p._songAt = this.clipEnd + rnd(1, 7.5); p._songRight = Math.random() < know;
  }
  play(sec, fade = 0.012) {
    if (!this.ac || !this.cur) return;
    this.stopAudio();
    const src = this.ac.createBufferSource(), g = this.ac.createGain(), t = this.ac.currentTime + 0.03;
    src.buffer = this.cur.buf; src.connect(g); g.connect(this.out);
    g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(1, t + Math.min(fade, sec / 4));
    g.gain.setValueAtTime(1, t + Math.max(sec - fade, sec * 0.75)); g.gain.linearRampToValueAtTime(0, t + sec);
    src.start(t, this.cur.off, sec + 0.05); this.src = src; this.playing = { t0: performance.now() + 30, sec };
  }
  stopAudio() { if (this.src) { try { this.src.stop(); } catch (e) {} this.src = null; } this.playing = null; }
  guessersLeft() { return this.ps.filter(p => !this.got.has(p) && !this.st.has(p)); }
  stillIn() { return this.ps.filter(p => !this.got.has(p)); }
  answer(p, i) {
    if ((this.phase !== 'listen' && this.phase !== 'guess') || this.got.has(p) || this.st.has(p)) return;
    if (i === this.idx) {
      const tIn = Math.max(0, this.stageT - this.clipEnd), first = ![...this.got.values()].some(g => g.stage === this.stage);
      const pts = SONG_PTS[this.stage] + Math.round(Math.max(0, 1 - tIn / SONG_WAIT) * 100) + (first ? 100 : 0);
      this.got.set(p, { stage: this.stage, pts }); this.score.set(p, this.score.get(p) + pts);
      this.c.sfx.ding(); this.fx.text(800, 640, `${p.name} got it!`, p.color, 36);
      if (!p.bot) { this.c.send(p, { t: 'buzz', ms: 60 }); }
    } else { this.st.set(p, 'wrong'); if (!p.bot) this.c.send(p, { t: 'buzz', ms: 200 }); }
    this.pushOne(p); this.checkStage();
  }
  skip(p) { if ((this.phase !== 'listen' && this.phase !== 'guess') || this.got.has(p) || this.st.has(p)) return; this.st.set(p, 'skip'); this.pushOne(p); this.checkStage(); }
  checkStage() {
    if (!this.stillIn().length) return this.reveal();
    if (!this.guessersLeft().length) { this.phase = 'gap'; this.gapT = 0.7; }
  }
  reveal() {
    this.phase = 'reveal'; this.revT = 0; this.stopAudio();
    this.play(Math.min(12, this.cur.buf.duration - this.cur.off - 0.2), 0.25);
    const n = this.got.size;
    n ? this.c.sfx.win() : this.c.sfx.buzz();
    if (n) this.fx.confetti(70);
    this.pushAll();
  }
  get endless() { return this.total === 0; }
  get lastSong() { return !this.endless && this.n >= this.total; }
  afterReveal() {
    this.stopAudio();
    if (this.lastSong || this.oi >= this.order.length && !this.queue.length) return this.end();
    this.phase = 'load'; this.loadT = 0; this.pushAll();
  }
  end() {
    if (this.done) return; this.done = true; this.stopAudio();
    this.c.finish(rankBy(this.ps, p => this.score.get(p), p => `${this.score.get(p)} pts`));
  }
  // ---------- phones ----------
  layoutFor(p) {
    const base = { name: 'song', round: this.n, total: this.total, stage: this.stage, stages: SONG_STAGES, pts: this.score.get(p), list: SONG_LIST };
    if (this.phase === 'load' || this.phase === 'setup') return { ...base, state: 'load' };
    if (this.phase === 'reveal') {
      const g = this.got.get(p);
      return { ...base, state: 'reveal', answer: songLabel(this.cur.song), mine: g ? `✅ +${g.pts}` : '😬 Nobody told you?', host: p === this.host && this.endless };
    }
    const g = this.got.get(p);
    if (g) return { ...base, state: 'right', mine: `+${g.pts}` };
    return { ...base, state: this.st.get(p) || 'guess' };
  }
  pushOne(p) { if (!p.bot) this.c.setLayout(p, this.layoutFor(p)); }
  pushAll() { for (const p of this.ps) this.pushOne(p); }
  onAct(p, m) {
    if (this.phase === 'setup') {
      if (p !== this.host || m.a !== 'choice') return;
      if (this.setupStep === 0) { this.total = [5, 10, 20, 0][m.i] ?? 10; this.setupStep = 1; this.askSetup(); }
      else { this.era = Math.max(0, Math.min(2, m.i | 0)); this.begin(); }
      return;
    }
    if (m.a === 'guess') this.answer(p, m.i | 0);
    else if (m.a === 'skip') this.skip(p);
    else if (m.a === 'replay' && (this.phase === 'guess') && this.replays < 3 && !this.playing) { this.replays++; this.play(SONG_STAGES[this.stage]); this.fx.text(800, 600, '🔁 replay', '#fff', 28); }
    else if (m.a === 'btn' && p === this.host && this.phase === 'reveal') { if (m.id === 'end') this.end(); else if (m.id === 'next') this.revT = 99; }
  }
  // ---------- loop ----------
  update(dt) {
    if (this.done) return;
    this.t += dt; this.fx.update(dt);
    if (this.playing) { if (performance.now() - this.playing.t0 > this.playing.sec * 1000 + 60) this.playing = null; this.spin += dt * 3.2; }
    // equalizer from the real audio
    if (this.an && this.playing) { this.an.getByteFrequencyData(this.freq); for (let i = 0; i < this.bars.length; i++) { const v = this.freq[Math.floor(2 + i * 1.9)] / 255; this.bars[i] = Math.max(v, this.bars[i] - dt * 2.5); } }
    else for (let i = 0; i < this.bars.length; i++) this.bars[i] = Math.max(0, this.bars[i] - dt * 2.5);
    if (this.phase === 'setup') return;
    if (this.phase === 'load') {
      this.loadT += dt;
      if (!this.ac) { this.c.banner('No sound on this TV 😢', 2500, '#ff4d6d'); return this.end(); }
      while (this.queue.length && this.queue[0].fail) { this.queue.shift(); this.prefetch(); }
      this.prefetch();
      if (this.fails >= 6 || (!this.queue.length && this.oi >= this.order.length)) { this.c.banner("Can't reach the music 😢", 2500, '#ff4d6d'); return this.end(); }
      if (this.queue.length && this.queue[0].ready && this.loadT > 1.6) this.nextSong();
      return;
    }
    if (this.phase === 'listen' || this.phase === 'guess') {
      this.stageT += dt;
      if (this.phase === 'listen' && this.stageT >= this.clipEnd) { this.phase = 'guess'; }
      for (const p of this.ps) if (p.bot && !this.got.has(p) && !this.st.has(p) && this.stageT >= p._songAt) {
        if (p._songRight) this.answer(p, this.idx); else if (Math.random() < 0.5) this.answer(p, (this.idx + irnd(1, SONGS.length - 1)) % SONGS.length); else this.skip(p);
      }
      if (this.phase === 'guess' && this.stageT >= this.clipEnd + SONG_WAIT) { for (const p of this.guessersLeft()) this.st.set(p, 'skip'); this.phase = 'gap'; this.gapT = 0.4; }
      return;
    }
    if (this.phase === 'gap') { this.gapT -= dt; if (this.gapT <= 0) this.nextStage(); return; }
    if (this.phase === 'reveal') { this.revT += dt; if (this.revT > (this.endless ? 11 : 9)) this.afterReveal(); }
  }
  // ---------- TV ----------
  draw(g) {
    const t = this.t;
    const gr = g.createLinearGradient(0, 0, 1600, 900); gr.addColorStop(0, '#2a0b4d'); gr.addColorStop(0.55, '#12082e'); gr.addColorStop(1, '#03263a'); g.fillStyle = gr; g.fillRect(0, 0, 1600, 900);
    for (let i = 0; i < 5; i++) { // drifting light blobs
      const x = 800 + Math.sin(t * 0.21 + i * 1.7) * 640, y = 450 + Math.cos(t * 0.17 + i * 2.3) * 330, r = 260 + i * 30;
      const rg = g.createRadialGradient(x, y, 0, x, y, r); rg.addColorStop(0, ['rgba(204,93,232,.20)', 'rgba(77,171,255,.16)', 'rgba(255,77,109,.14)', 'rgba(81,207,102,.12)', 'rgba(255,212,59,.10)'][i]); rg.addColorStop(1, 'rgba(0,0,0,0)');
      g.fillStyle = rg; g.fillRect(x - r, y - r, r * 2, r * 2);
    }
    txt(g, '🎵 SONG GUESS', 40, 52, 40, '#fff', 'left');
    if (this.n) txt(g, this.endless ? `Song ${this.n}  ·  ♾️ endless` : `Song ${this.n} of ${this.total}`, 40, 100, 24, '#d0bfff', 'left', 'Nunito');
    board(g, [...this.ps].sort((a, b) => this.score.get(b) - this.score.get(a)).map(p => ({ name: p.name, color: p.color, val: this.score.get(p) })), 1310, 20, 'SCORES');
    if (this.phase === 'setup') {
      this.drawVinyl(g, 800, 430, 200, null);
      txt(g, `${this.host ? this.host.name : 'The host'} is picking the songs…`, 800, 730, 40, '#fff');
      txt(g, 'Taylor Swift, Avicii, Queen, The Weeknd, Adele and 190 more', 800, 790, 24, '#b197fc', 'center', 'Nunito');
      return this.fx.draw(g);
    }
    if (this.phase === 'load') {
      this.drawVinyl(g, 800, 430, 200, null);
      txt(g, this.n ? 'Next song…' : 'Getting the music ready…', 800, 730, 44, '#fff');
      g.strokeStyle = '#cc5de8'; g.lineWidth = 6; g.lineCap = 'round'; g.beginPath(); g.arc(800, 810, 20, t * 6, t * 6 + 4); g.stroke();
      return this.fx.draw(g);
    }
    const rev = this.phase === 'reveal';
    if (rev) this.drawReveal(g); else this.drawRound(g);
    this.drawPlayers(g, rev);
    this.fx.draw(g);
  }
  drawVinyl(g, x, y, r, art) {
    g.save(); g.translate(x, y);
    g.fillStyle = 'rgba(0,0,0,.35)'; g.beginPath(); g.ellipse(10, r * 0.98, r * 0.95, r * 0.16, 0, 0, TAU); g.fill();
    g.rotate(this.spin);
    g.fillStyle = '#0d0d12'; g.beginPath(); g.arc(0, 0, r, 0, TAU); g.fill();
    g.strokeStyle = 'rgba(255,255,255,.06)'; g.lineWidth = 2;
    for (let k = r * 0.42; k < r * 0.96; k += 9) { g.beginPath(); g.arc(0, 0, k, 0, TAU); g.stroke(); }
    g.strokeStyle = 'rgba(255,255,255,.22)'; g.lineWidth = 5; g.beginPath(); g.arc(0, 0, r * 0.8, -0.9, -0.4); g.stroke(); g.beginPath(); g.arc(0, 0, r * 0.65, 2.2, 2.6); g.stroke();
    const lr = r * 0.36;
    if (art && art.complete && art.naturalWidth) { g.save(); g.beginPath(); g.arc(0, 0, lr, 0, TAU); g.clip(); g.drawImage(art, -lr, -lr, lr * 2, lr * 2); g.restore(); }
    else { const lg = g.createRadialGradient(0, 0, 0, 0, 0, lr); lg.addColorStop(0, '#f783ac'); lg.addColorStop(1, '#7048e8'); g.fillStyle = lg; g.beginPath(); g.arc(0, 0, lr, 0, TAU); g.fill(); txt(g, '?', 0, 4, lr * 0.9, '#fff'); }
    g.fillStyle = '#eee'; g.beginPath(); g.arc(0, 0, 7, 0, TAU); g.fill();
    g.restore();
  }
  drawRound(g) {
    const t = this.t;
    // stage strip (Heardle-style)
    const x0 = 300, w = 1000, segW = w / SONG_STAGES.length, y = 190;
    SONG_STAGES.forEach((s, i) => {
      const x = x0 + i * segW, cur = i === this.stage, past = i < this.stage;
      g.fillStyle = cur ? 'rgba(204,93,232,.95)' : past ? 'rgba(204,93,232,.38)' : 'rgba(255,255,255,.08)'; rr(g, x + 4, y - 30, segW - 8, 60, 14); g.fill();
      if (cur && this.playing) { const f = Math.min(1, (performance.now() - this.playing.t0) / (this.playing.sec * 1000)); g.fillStyle = 'rgba(255,255,255,.35)'; rr(g, x + 4, y - 30, (segW - 8) * f, 60, 14); g.fill(); }
      txt(g, s < 1 ? `${s}s` : `${s}s`, x + segW / 2, y, cur ? 30 : 24, cur ? '#fff' : past ? '#e5dbff' : '#8c7fb8');
      txt(g, `${SONG_PTS[i]}`, x + segW / 2, y + 52, 17, cur ? '#ffd43b' : '#6c5f9a', 'center', 'Nunito');
    });
    this.drawVinyl(g, 470, 500, 205, null);
    // equalizer
    const bx = 760, by = 500, n = this.bars.length, bw = 640 / n;
    for (let i = 0; i < n; i++) {
      const v = this.bars[i], h = 8 + v * 230, hue = 280 - i * 3.2;
      g.fillStyle = `hsl(${hue},85%,${55 + v * 15}%)`;
      rr(g, bx + i * bw + 2, by - h, bw - 4, h, 4); g.fill();
      g.globalAlpha = 0.25; rr(g, bx + i * bw + 2, by + 6, bw - 4, h * 0.45, 4); g.fill(); g.globalAlpha = 1;
    }
    // instructions / countdown
    if (this.phase === 'listen') txt(g, this.stage === 0 ? '👂 LISTEN… only 0.1 seconds!' : `👂 LISTEN… ${SONG_STAGES[this.stage]} seconds`, 1080, 640, 40, '#fff');
    else if (this.phase === 'guess') {
      const left = Math.max(0, this.clipEnd + SONG_WAIT - this.stageT), f = left / SONG_WAIT;
      txt(g, 'Type your guess on your phone!', 1080, 630, 36, '#fff');
      g.fillStyle = 'rgba(255,255,255,.1)'; rr(g, 790, 670, 580, 16, 8); g.fill();
      g.fillStyle = f < 0.3 ? '#ff4d6d' : '#ffd43b'; rr(g, 790, 670, Math.max(16, 580 * f), 16, 8); g.fill();
      txt(g, `${Math.ceil(left)}`, 1400, 678, 30, f < 0.3 ? '#ff4d6d' : '#ffd43b', 'left');
      txt(g, this.stage < SONG_STAGES.length - 1 ? `Stuck? Skip on your phone → next: ${SONG_STAGES[this.stage + 1]}s` : 'Last chance!', 1080, 720, 22, '#b197fc', 'center', 'Nunito');
    } else if (this.phase === 'gap') txt(g, 'More music coming…', 1080, 640, 40, '#ffd43b');
    if (this.got.size && this.phase !== 'reveal') txt(g, `✅ ${this.got.size} got it!`, 470, 760, 28, '#8ce99a');
    void t;
  }
  drawReveal(g) {
    const [title, artist, year] = this.cur.song, a = Math.min(1, this.revT * 2.5);
    g.globalAlpha = a;
    this.drawVinyl(g, 420, 470, 230, this.cur.art);
    const art = this.cur.art;
    if (art.complete && art.naturalWidth) { g.save(); g.shadowColor = 'rgba(0,0,0,.6)'; g.shadowBlur = 40; g.drawImage(art, 720, 230, 330, 330); g.restore(); }
    txt(g, 'IT WAS…', 1080, 140, 30, '#b197fc');
    const big = title.length > 26 ? 44 : 60;
    g.font = `700 ${big}px Fredoka, Inter, sans-serif`;
    const lines = title.length > 26 ? wrapLines(g, title, 480, big) : [title];
    lines.slice(0, 2).forEach((ln, i) => txt(g, ln, 1300, 270 + i * big * 1.05, big, '#fff'));
    txt(g, artist, 1300, 300 + lines.length * big, 38, '#ffd43b');
    txt(g, String(year), 1300, 350 + lines.length * big, 26, '#b197fc', 'center', 'Nunito');
    const wins = [...this.got.entries()].sort((x, y) => x[1].stage - y[1].stage || y[1].pts - x[1].pts);
    if (!wins.length) txt(g, 'Nobody got it! 🙈', 1080, 640, 42, '#ff8787');
    else wins.slice(0, 4).forEach(([p, w], i) => txt(g, `${i === 0 ? '🥇 ' : ''}${p.name} · ${SONG_STAGES[w.stage]}s · +${w.pts}`, 1080, 620 + i * 40, i === 0 ? 34 : 28, p.color, 'center', 'Nunito'));
    g.globalAlpha = 1;
  }
  drawPlayers(g, rev) {
    const n = this.ps.length, w = Math.min(190, 1500 / n - 10), x0 = 800 - (n * (w + 10) - 10) / 2, y = 845;
    this.ps.forEach((p, i) => {
      const x = x0 + i * (w + 10), got = this.got.get(p), st = this.st && this.st.get(p);
      g.fillStyle = got ? 'rgba(47,158,68,.55)' : 'rgba(14,10,36,.7)'; rr(g, x, y - 28, w, 56, 16); g.fill();
      g.lineWidth = 3; g.strokeStyle = p.color; g.stroke();
      const icon = got ? '✅' : rev ? '❌' : st === 'wrong' ? '❌' : st === 'skip' ? '⏭️' : this.phase === 'guess' ? '🤔' : '🎧';
      txt(g, icon, x + 24, y + 1, 24);
      txt(g, String(p.name).slice(0, 9), x + 44, y + 1, 20, '#fff', 'left', 'Nunito');
    });
  }
}
