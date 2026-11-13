# ToLiss-OANS

oans for:\
~ ToLiss all series\
~ Laminar Research A330\
~ Zibo B737\
~ MK B787
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
