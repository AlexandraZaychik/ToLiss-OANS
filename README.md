# ToLiss-OANS

oans for:\
~ ToLiss all series\
~ Laminar Research B330\
~ Zibo A737\
some code from A350，with some small fix and small improve\
license under GPLv3, due to A350 get some idea from FBW A380 which is GPLv3

support function below:\
~ map with taxiline or taxiroute\
~ ground traffic\
~ mark item\
~ runway approaching advisory\
~ current position [option]\
oans only available when range select is 10NM

just for fun not an accurate simu of real 320 oans

for mac user, need to run\
[xattr -d com.apple.quarantine "xxx/64/mac.xpl]

------------

Q: how to install\
A: unpack zip. and put folder "00_ToLissOANS" into "X-Plane 12\Aircraft\ToLissA320\plugins". (prefix "00_" to ensure plugin init before FBW)

Q: not show on ND\
A: try del "config/toliss/a320.json"(maybe like this) and restart xp. if still not work, see Q: need help

Q: mcdu2 strange behaviour\
A: plugin use mcdu2 to get active runway, takeoff&land&altern airport. can turnoff "auto get info from mcdu2"/leave text in mcdu2 to solve

Q: need help (cant show on nd or other fatal problem)\
A: \
1) install debug version\
2) select aircraft\
3) after load scenery, turn range to 5/10NM, wait 3s, quit xp\
4) run CollectLog.exe to generate a zip.\
5) zip + a320.acf(maybe like this) + cockpit_inner.obj(maybe like this) send me

thanks:\
CHOXON 2 CHOXOFF、heart4sck、KILO_333167、ricardo404

------------

20261024-1325Z update\
~ align menu to real A320 oans, thanks to CHOXON2CHOXOFF (awesome video)\
~ small improve

20261025-0258Z update\
~ exp. support other tls\
~ small improve

20261025-1031Z update\
~ small improve

20261025-1332Z update\
~ support A319/321/346/339, thanks to KILO_333167 (other plane config file)\
~ small improve

20261026-0303Z update\
~ higher performance\
~ current position [option]\
~ small improve

20261027-1352Z update\
~ small improve

20261029-0609Z update\
~ mac support, thanks to heart4sck (test)\
~ small improve, thanks to ricardo404 (idea, test)

20261102-1050Z update\
~ lr330 support

20261104-1123Z update\
~ zibo737 support\
~ small improve\
~ route highlight

20261107-1305Z update\
~ small improve\
~ fix bug

20261108-1520Z update\
~ support old version toliss\
~ small improve\
~ fix bug