Technograph OPC Server v0.57.34

overview:
---------
Technograph used single data recieve command. Worked with three ports.
Server based on lightopc v0.88. Software is absolutely free.

installation notes:
-------------------
After install software you must register server with key /r.
You also may unregistered server with key /u.
For example tg160srv.exe /r or tg160srv.exe /u.
Show help key /?.

version history:
----------------
v0.57 build 34
+ add to config file technograph configuration. if technograph switch off before
server start, he will work normal later.
- delete tags with time and data

v0.55 build 12
+ add ability work with 3 com-ports
= change serial communication function
= reduce waiting answer time
= reduce timeout settings of com-port
- server now don`t use MFC library, but still request their ))

v0.41 build 14
= return ability set "incertain" on tags, when device switch off.
- disable many log-file events

v0.41 build 3
- disable many log-file events

v0.40 build 32
= increase recieve data timeout to 500ms.
0 fixed bug, when disabled channal brings to channal shifting. 
Number now has been taken directly from recieve data.
0 fixed locale settings bug. Decimal comma not accepted by atof function.
Decimal comma now equal decimal point on any workstation.