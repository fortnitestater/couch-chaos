/* Optional relay (TURN) server, so phones on mobile data or another network can reach the TV.
   Free option: make an account at https://www.metered.ca/stun-turn , create an app, then paste the
   "credentials API" link here, e.g.
   window.CC_TURN_URL = 'https://YOURAPP.metered.live/api/v1/turn/credentials?apiKey=YOUR_KEY';
   Leave it empty to use direct connections only (works when everyone is on the same Wi-Fi). */
window.CC_TURN_URL = '';
