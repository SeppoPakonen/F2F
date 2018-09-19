TABLE_(USERS)
   INT_     (ID) PRIMARY_KEY
   STRING   (NICK, 200)
   INT_     (PASSHASH)
   TIME_    (JOINED)
   TIME_    (LASTLOGIN)
   INT_     (LOGINS)
   INT_     (ONLINETOTAL)
   INT_     (VISIBLETOTAL)
END_TABLE

TABLE_(CHANNELS)
   INT_     (CH_USER_ID) REFERENCES(USERS.ID)
   STRING_  (CHANNEL, 200)
   TIME_    (CH_JOINED)
END_TABLE

TABLE_(LAST_LOCATION)
   INT_     (LL_USER_ID) REFERENCES(USERS.ID)
   DOUBLE_  (LL_LON)
   DOUBLE_  (LL_LAT)
   DOUBLE_  (LL_ELEVATION)
   TIME_    (LL_UPDATED)
END_TABLE

TABLE_(LOCATION_HISTORY)
   INT_     (LH_USER_ID) REFERENCES(USERS.ID)
   DOUBLE_  (LH_LON)
   DOUBLE_  (LH_LAT)
   DOUBLE_  (LH_ELEVATION)
   TIME_    (LH_UPDATED)
END_TABLE
