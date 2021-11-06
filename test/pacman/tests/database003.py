self.description = "-D --note :D"

lp = pmpkg("pkg")
self.addpkg2db("local", lp)

self.args = "-D pkg --note :D"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg")
self.addrule("PKG_NOTE=pkg|:D")
