/* Couch Chaos — announcer voice (ElevenLabs "Tom", British event announcer).
   One recording holds every call; CLIPS says where each line starts and ends (seconds).
   SFX.say() plays these clips when the text matches, otherwise falls back to the device voice. */
const VOICE = {
  url: 'audio/announcer.mp3',
  lines: ['love all', 'fifteen love', 'love fifteen', 'fifteen all', 'thirty love', 'love thirty', 'thirty fifteen', 'fifteen thirty', 'thirty all',
    'forty love', 'love forty', 'forty fifteen', 'fifteen forty', 'forty thirty', 'thirty forty', 'deuce', 'advantage red', 'advantage blue',
    'advantage server', 'advantage receiver', 'fault', 'double fault', 'out', 'net', 'ace', 'game red', 'game blue', 'game',
    'game set and match red', 'game set and match blue', 'game set and match', 'red to serve play', 'blue to serve play', 'quiet please',
    'what a shot', 'unbelievable', 'smash what power', 'strike', 'spare', 'home run', 'gutter ball'],
  // measured from the recording's silences (41 lines → 41 clips)
  clips: [[0,0.56],[1.16,1.82],[2.5,3.22],[3.84,4.46],[5.14,5.76],[6.36,7],[7.76,8.46],[9.1,10.02],[10.72,11.28],[11.9,12.52],[13.12,13.72],[14.34,15.1],[15.66,16.36],[17.08,17.72],[18.44,19.1],[19.74,20.16],[20.86,21.88],[22.32,22.92],[23.6,24.2],[24.88,25.48],[26.18,26.46],[27.12,27.56],[28.3,28.58],[30.14,30.52],[31.2,31.76],[32.4,32.94],[33.58,33.96],[34.6,35.62],[36.26,37.32],[37.92,38.7],[39.88,41.06],[42.32,43.48],[44.62,45.48],[46.7,47.46],[48.6,49.4],[50.5,51.26],[52.42,53.2],[54.28,54.92],[56,56.68],[57.8,58.4],[59.5,60.14]],
};
