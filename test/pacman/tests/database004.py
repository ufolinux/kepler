self.description = "-D --rmnote"

lp = pmpkg("pkg")
lp.note = "D:"
self.addpkg2db("local", lp)

self.args = "-D pkg --rmnote"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg")
self.addrule("PKG_NOTE=pkg|")
